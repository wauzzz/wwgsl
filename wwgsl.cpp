#include <iostream>
#include <fstream>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <getopt.h>
#include <iomanip>

#define GAPI_KEY "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

using json = nlohmann::json;
using namespace std;

class WigleJSON{
public:
	WigleJSON(string file);
	bool isLoaded(){ return this->_isLoaded; }
	void removeDuplicates();
	bool validateBssList();
	bool sortByDistance(double distance);
	bool saveJSONToFile(string filename);
	bool _googleGetLocation(int *bssList, int listCount, int iStrengthPoint, double &retlat, double &retlon);

private:
	~WigleJSON();
	json _jsonWigle;
	bool _isLoaded = false;
	double _averageLat = 0.0, _averageLon = 0.0;
	double _distanceEarth(double lat1d, double lon1d, double lat2d, double lon2d);
};

WigleJSON::WigleJSON(string file){
	ifstream fWigle;
	fWigle.open(file);
	if(fWigle.is_open()){
		fWigle >> _jsonWigle;
		_isLoaded = true;
		fWigle.close();
		int resultCount = _jsonWigle["resultCount"];
		cout << "resultCount: " << resultCount << endl;
		for(int i = 0; i < resultCount; ++i){
			_averageLat += (double)(_jsonWigle["results"][i]["trilat"]) / resultCount;
			_averageLon += (double)(_jsonWigle["results"][i]["trilong"]) / resultCount;
		}
		cout << "average Lat,Lon: " << setprecision(7) << _averageLat <<"," << _averageLon << endl;
	}
}

void WigleJSON::removeDuplicates(){
	int resultCount = _jsonWigle["resultCount"];
	for(int i = 0; i < resultCount; ++i){
		for(int j = i+1; j < _jsonWigle["resultCount"]; ++j){
			if(_jsonWigle["results"][i]["netid"] == _jsonWigle["results"][j]["netid"]){
				_jsonWigle["results"].erase(j);
				resultCount--;
				j--;
				cout << "removed 1 duplicate entry\n";
			}
		}
	}
	_jsonWigle["resultCount"] = resultCount;
}

bool WigleJSON::validateBssList(){
	int bssList[100];
	bssList[0] = 0;
	int resultCount = _jsonWigle["resultCount"];
	int firstGoodPoint = -1, secondGoodPoint = -1;
	double retlat, retlon;
	int goodPoints[100] = {0};
	for(int i = 1; i < resultCount; ++i){
		bssList[i] = i;
		if(_googleGetLocation(bssList, i+1, i, retlat, retlon)){
			cout << "Lat,Lon: " << retlat << "," << retlon << endl;
			firstGoodPoint = i;
			_jsonWigle["results"][i]["trilat"] = retlat;
			_jsonWigle["results"][i]["trilong"] = retlon;
			goodPoints[i] = 1;
			cout << _jsonWigle["results"][i] << endl;
			break;
		}
	}
	if(firstGoodPoint != -1){
		bssList[0] = firstGoodPoint;
		for(int i = 0; i < resultCount; ++i){
			if(i == firstGoodPoint) continue;
			bssList[1] = i;
			if(_googleGetLocation(bssList, 2, 1, retlat, retlon)){
				cout << "Lat,Lon: " << retlat << "," << retlon << endl;
				secondGoodPoint = i;
				_jsonWigle["results"][i]["trilat"] = retlat;
				_jsonWigle["results"][i]["trilong"] = retlon;
				goodPoints[i] = 1;
				cout << _jsonWigle["results"][i] << endl;
				break;
			}
		}
		if(secondGoodPoint != -1){
			for(int i = secondGoodPoint + 1; i < resultCount; ++i){
				bssList[0] = firstGoodPoint;
				if(i == firstGoodPoint) bssList[0] = secondGoodPoint;
				bssList[1] = i;
				if(_googleGetLocation(bssList, 2, 1, retlat, retlon)){
					cout << "Lat,Lon: " << retlat << "," << retlon << endl;
					secondGoodPoint = i;
					_jsonWigle["results"][i]["trilat"] = retlat;
					_jsonWigle["results"][i]["trilong"] = retlon;
					goodPoints[i] = 1;
					cout << _jsonWigle["results"][i] << endl;
				}
			}
			int j = resultCount;
			for(int i = resultCount -1; i >= 0; --i){
				if(!goodPoints[i]){
					_jsonWigle["results"].erase(i);
					j--;
				}
			}
			_jsonWigle["resultCount"] = j;
			return true;
		}
	}
	return false;
}

bool WigleJSON::sortByDistance(double distance){
	bool bret = false;
	int numPoints[100] = {0};
	char pointSet[100][100] = {0};
	json jsonTemp;
	int resultCount = _jsonWigle["resultCount"];
	for(int i = 0; i < resultCount; ++i){
		for(int j = 0; j < resultCount; ++j){
			if(i == j) continue;
			if(_distanceEarth(_jsonWigle["results"][i]["trilat"],_jsonWigle["results"][i]["trilong"],_jsonWigle["results"][j]["trilat"],_jsonWigle["results"][j]["trilong"]) < distance){
				numPoints[i]++;
				pointSet[i][j] = 1;
				bret = true;
			}
		}
	}
	if(bret){
		int maxCount = 0;
		int imax;
		for(int i = 0; i < resultCount; ++i){
			if(numPoints[i] > maxCount){
				maxCount = numPoints[i];
				imax = i;
			}
		}
		jsonTemp = _jsonWigle;
		jsonTemp["results"].clear();
		jsonTemp["results"][0] = _jsonWigle["results"][imax];
		int j = 1;
		for(int i = 0; i < resultCount; i++){
			if(pointSet[imax][i]){
				jsonTemp["results"][j] = _jsonWigle["results"][i];
				j++;
			}
		}
		jsonTemp["resultCount"] = j;
		_jsonWigle = jsonTemp;
		jsonTemp.clear();
	}
	return bret;
}

bool WigleJSON::saveJSONToFile(string filename){
	ofstream fWigle;
	fWigle.open(filename);
	if(fWigle.is_open()){
		fWigle << setw(2) << _jsonWigle;
		return true;
	}
	return false;
}

bool WigleJSON::_googleGetLocation(int *bssList, int listCount, int iStrengthPoint, double &retlat, double &retlon){
	json gapiReqJSON;
	CURL *curl = curl_easy_init();
	if(curl){
	    char data[65536];
        char res_data[512];
        FILE* f_res = fmemopen(res_data, 512, "w+");
    	json req_json, res_json;
    	req_json["considerIp"] = "false";
    	for(int i = 0; i < listCount; ++i){
    		req_json["wifiAccessPoints"][i]["macAddress"] = _jsonWigle["results"][bssList[i]]["netid"];
        	int _sig;
    		if(i == iStrengthPoint) _sig = -35; else _sig = -100;
    		req_json["wifiAccessPoints"][i]["signalStrength"] = _sig;
    		req_json["wifiAccessPoints"][i]["signalToNoiseRatio"] = 0;
    	}
    	sprintf(data, "%s", req_json.dump().data());
    	req_json.clear();
		curl_slist* list = curl_slist_append(list, "Content-Type: application/json");
	    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/geolocation/v1/geolocate?key=" GAPI_KEY);
	    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f_res);
    	cout << "google API request\n";
    	CURLcode res = curl_easy_perform(curl);
    	fclose(f_res);
	    curl_slist_free_all(list);
	    curl_easy_cleanup(curl);
    	if(res != CURLE_OK){
    		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    		return false;
    	}
    	res_json = json::parse(res_data);
    	if(!res_json["error"].is_null()){
    		res_json.clear();
    		cout << "not found\n";
    		return false;
    	}else if(!res_json["location"]["lat"].is_null()){
    		retlat = res_json["location"]["lat"];
    		retlon = res_json["location"]["lng"];
    		res_json.clear();
    		return true;
    	}
	}
	return false;
}

WigleJSON::~WigleJSON(){
	_jsonWigle.clear();
}

double WigleJSON::_distanceEarth(double lat1d, double lon1d, double lat2d, double lon2d) {
#define earthRadius 6371000.0
#define _deg2rad(deg) (deg * M_PI / 180.0)
//#define _rad2deg(rad) (rad * 180.0 / M_PI)

  double lat1r, lon1r, lat2r, lon2r, u, v;
  lat1r = _deg2rad(lat1d);
  lon1r = _deg2rad(lon1d);
  lat2r = _deg2rad(lat2d);
  lon2r = _deg2rad(lon2d);
  u = sin((lat2r - lat1r)/2);
  v = sin((lon2r - lon1r)/2);
  return 2.0 * earthRadius * asin(sqrt(u * u + cos(lat1r) * cos(lat2r) * v * v));
}



int main(int argc, char **argv) {
	double distance = 50,_distance;
	string outfile = "";
    int c;
    int digit_optind = 0;
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"distance",required_argument, 0, 'd'},
            {"outfile", required_argument, 0, 'o'},
            {"help",    no_argument,       0, 'h'},
            {0,         0,                 0,  0 }
        };
        c = getopt_long(argc, argv, "hd:o:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'h':
        	cout << "USAGE\n";
        	cout << "  google-geo [OPTIONS] <input-file>\n";
        	cout << "OPTIONS:\n";
        	cout << "  --distance|-d <METERS>    0 - do not sort by distance, default: 50 meters\n";
        	cout << "  --outfile|-o <filename>   default: <input-file> + '.out'\n";
        	cout <<	"  --help|-h\n";
        	return 0;
        case 'd':
            _distance = strtod(optarg,0);
            if(_distance > 0) distance = _distance;
            else if(!strcmp(optarg,"0")) distance = HUGE_VAL;
			else{
            	cout << "invalid value of --distance '" << optarg << "' specified\n";
            	return 1;
            }
            break;
        case 'o':
            outfile = optarg;
            break;
        case '?':
            return 1;
        default:
            cout << "?? getopt returned character code '" << c <<  "' ??\n";
        }
    }
    if (!(optind < argc)) {
    	cout << "input filename has no specified\n";
    	return 1;
    }
    if(outfile == "") {outfile = argv[optind]; outfile += ".out";}


	curl_global_init(CURL_GLOBAL_ALL);
	WigleJSON *wj;
	cout << argv[optind] << endl;
	wj = new WigleJSON(argv[optind]);
	if(wj->isLoaded()) cout << "json is loaded\n";
	wj->removeDuplicates();
	wj->validateBssList();
	wj->sortByDistance(distance);
	wj->saveJSONToFile(outfile);
	curl_global_cleanup();
	return 0;
}
