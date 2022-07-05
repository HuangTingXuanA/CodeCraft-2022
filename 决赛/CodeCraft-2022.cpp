#include <bits/stdc++.h>
using namespace std;

#define DEBUG

#ifdef DEBUG
	#define	demand_path "../MyWorkC++/CodeCraft/final/data/demand.csv"
	#define	site_bandwidth_path "../MyWorkC++/CodeCraft/final/data/site_bandwidth.csv"
	#define	qos_path "../MyWorkC++/CodeCraft/final/data/qos.csv"
	#define	config_path "../MyWorkC++/CodeCraft/final/data/config.ini"
	#define	output_path "../MyWorkC++/CodeCraft/final/data/solution.txt"
	#define MAX_TIME 5
#else
	#define	demand_path "/data/demand.csv"
	#define	site_bandwidth_path "/data/site_bandwidth.csv"
	#define	qos_path "/data/qos.csv"
	#define	config_path "/data/config.ini"
	#define	output_path "/output/solution.txt"
	#define MAX_TIME 60
#endif

#define N_c_MAX (35)
#define N_s_MAX (135)
#define N_t_MAX (8928)
#define N_stream_MAX (100)

ifstream fin;
ofstream fout;

unordered_map<string, int> n2id_c;
vector<string> id2n_c;//id_c to name_c

unordered_map<string, int> n2id_s;
vector<string> id2n_s;//id_s to name

vector<int> site_bandwidth;//id_s to band_max
set<string> n_stream_set;//stream name set

int qos_constraint;//max_qos
int base_cost;//base_cost
double center_cost;

int N_c, N_s, N_t;
bool qos[N_s_MAX][N_c_MAX];

class Center_site
{
public:
	int big_size = 0;// N_t/20
	int small_size = 0;// N_t - N_t/20
	set<pair<int, int>, less<pair<int, int>>> big_set;
	set<pair<int, int>, greater<pair<int, int>>> small_set;
	vector<vector<vector<set<pair<int, int>, greater<pair<int, int>>>>>> flow_c;//t & s & str to pair<flow,id_c>

	Center_site(int _N_t, int _N_s) {
		flow_c.resize(_N_t);
		for (auto &flow_c_in_t : flow_c) {
			flow_c_in_t.resize(_N_s);
			for (auto &flow_c_vec_t_s : flow_c_in_t) {
				flow_c_vec_t_s.resize(N_stream_MAX);
			}
		}
		big_size = _N_t / 20;
		small_size = _N_t - big_size;
	}
	
	int get_max_flow(int t, int id_s, int stream_id) {
		if (flow_c[t][id_s][stream_id].empty()) {
			return 0;
		}
		auto it = flow_c[t][id_s][stream_id].begin();
		return it->first;
	}

	void set_erase(const pair<int, int> &usedband_t) {
		big_set.erase(usedband_t);
		small_set.erase(usedband_t);
	}

	void set_emplace(const pair<int, int> &usedband_t) {
		if (small_set.size() < small_size) {
			small_set.emplace(usedband_t);
		}
		else {
			big_set.emplace(usedband_t);
		}
		if (big_set.size() > 0) {
			auto it_small = small_set.begin();
			auto it_big = big_set.begin();
			if (it_small->first > it_big->first) {
				pair<int, int> p_small(it_small->first, it_small->second);
				pair<int, int> p_big(it_big->first, it_big->second);
				big_set.erase(p_big);
				big_set.insert(p_small);
				small_set.erase(p_small);
				small_set.insert(p_big);
			}
		}
	}

    int get_score_band() {
		auto it = small_set.begin();
        return it->first;
    }
    int get_score_t() {
		auto it = small_set.begin();
        return it->second;
    }
};

class Demand_t
{
public:
	int stream_num = 0;
	vector<string> stream_name;//stream_id to stream_name
	vector<vector<int>> demand;//stream_id & id_c to demand
	vector<vector<int>> destination;//stream_id & id_c to tar_id_s
	vector<vector<int>> ans;//stream_id & id_c to tar_id_s (best destination)
};
vector<Demand_t> demand_t_vec;//t to all_demand

class Usedband_t_set
{
public:
	int big_size = 0;// N_t/20
	int small_size = 0;// N_t - N_t/20
	set<pair<int, int>, less<pair<int, int>>> big_set;
	set<pair<int, int>, greater<pair<int, int>>> small_set;

    bool is_free() {
		auto it = big_set.end();
		it--;
        return it->first == 0;
    }

    int get_score_band() {
		auto it = small_set.begin();
        return it->first;
    }

    int get_state(const pair<int, int> &usedband_t) {
        if (big_set.find(usedband_t) != big_set.end())
            return 1;
        else {
            if (get_score_band() == usedband_t.first) {
                if (usedband_t.first == (*big_set.begin()).first) {
					return 1;
				} 
                else {
					return 2;
				}
            }
        }
        return 3;
    }

	void get_migrate_small_t_vec(vector<int> &migrate_t_vec) {
		auto it = small_set.begin();
		int score_band = it->first;
		if (score_band > 0) {
			while (1) {
				migrate_t_vec.emplace_back(it->second);
				if (it == small_set.end()) {
					break;
				}
				it++;
				if (it->first < score_band) {
					break;
				}
			}
		}
	}

	void get_migrate_big_t_vec(vector<int> &migrate_t_vec) {
		for (auto it: big_set) {
			if (it.first > 0) {
				migrate_t_vec.emplace_back(it.second);
			}
        }
	}

	void erase(const pair<int, int> &usedband_t) {
		big_set.erase(usedband_t);
		small_set.erase(usedband_t);
	}

	void emplace(const pair<int, int> &usedband_t) {
		if (small_set.size() < small_size) {
			small_set.emplace(usedband_t);
		}
		else {
			big_set.emplace(usedband_t);
		}

        auto it_small = small_set.begin();
        auto it_big = big_set.begin();

        if (it_small->first > it_big->first) {

			pair<int, int> p_small(it_small->first, it_small->second);
			pair<int, int> p_big(it_big->first, it_big->second);

			big_set.erase(p_big);
			big_set.insert(p_small);

			small_set.erase(p_small);
            small_set.insert(p_big);
        }

	}

};

void add(vector<vector<int>> &cache_percent, vector<Demand_t> &demand_t_vec, vector<Usedband_t_set> &used_t_set, Center_site &center_site, vector<vector<int>> &used_band, vector<vector<int>> &cache_band, vector<int> &center_used_band, int t, int stream_id, int id_c, int id_s) {
	Demand_t &demand_t = demand_t_vec[t];

	int band = demand_t.demand[stream_id][id_c];

	demand_t.destination[stream_id][id_c] = id_s;


	used_t_set[id_s].erase(pair<int, int>(used_band[t][id_s] + cache_band[t][id_s], t));
	used_band[t][id_s] += band;
	used_t_set[id_s].emplace(pair<int, int>(used_band[t][id_s] + cache_band[t][id_s], t));
	for (int _t = t + 1; _t < N_t; _t++) {
		
		int new_cache = (int)((cache_band[_t - 1][id_s] +  used_band[_t - 1][id_s]) * cache_percent[t][id_s] / 100);

		if (new_cache == cache_band[_t][id_s]) {
			break;
		}

		used_t_set[id_s].erase(pair<int, int>(used_band[_t][id_s] + cache_band[_t][id_s], _t));

		cache_band[_t][id_s] = new_cache;
		
		used_t_set[id_s].emplace(pair<int, int>(used_band[_t][id_s] + cache_band[_t][id_s], _t));
	}


	int center_max_flow = center_site.get_max_flow(t, id_s, stream_id);
	if (band > center_max_flow) {
		center_site.set_erase(pair<int, int>(center_used_band[t], t));
		center_used_band[t] += (band - center_max_flow);
		center_site.set_emplace(pair<int, int>(center_used_band[t], t));
	}
	center_site.flow_c[t][id_s][stream_id].emplace(band, id_c);
}

double cul_cost(int W,int C) {
	if (W <= base_cost) {
		return base_cost;
	}
	double temp = W - base_cost;
	temp = temp * temp / C;   
	temp += W;
	return temp;
}

int calculate_cost(vector<Usedband_t_set> &used_t_set, Center_site &center_site) {
	double cost_sum = 0.5;
	for (int id_s = 0; id_s < N_s; id_s++) {
		if (used_t_set[id_s].big_set.size() != 0 && !used_t_set[id_s].is_free()) {
			#ifdef DEBUG
				if (used_t_set[id_s].big_set.size() != 0 && !used_t_set[id_s].is_free()) {
					int val = cul_cost(used_t_set[id_s].get_score_band(), site_bandwidth[id_s]);
					cost_sum += val;
					printf("%d,", val);
				}
				if (id_s != 0 && id_s % 15 == 0) puts("");
				if (id_s == N_s - 1) puts("");
			#else
				cost_sum += cul_cost(used_t_set[id_s].get_score_band(), site_bandwidth[id_s]);
			#endif
		}
	}
	cout<<"site_cost = "<<cost_sum<<endl;
	cost_sum += center_cost * center_site.get_score_band();
	cout<<"center_cost = "<<(center_cost * center_site.get_score_band())<<endl;
	return cost_sum;
}

int min_cost = INT_MAX;
int t_once_max = 0;

int main() {

    time_t  start_time = time(NULL);

    {//config.ini
        string temp;
        fin.open(config_path, ios_base::in | ios_base::binary);
        if(!fin) {cerr << "未能打开txt文件" << endl; exit(-1);}
		getline(fin,temp);
        while (getline(fin,temp)) {
			if (temp.size() > 0) {
				int pos = temp.find('=');
				if (temp.substr(0, pos) == "qos_constraint"){
					qos_constraint = stoi(temp.substr(pos + 1, temp.size() - pos));
				}
				else if (temp.substr(0, pos) == "base_cost"){
					base_cost = stoi(temp.substr(pos + 1, temp.size() - pos));
				}
				else if (temp.substr(0, pos) == "center_cost"){
					center_cost = stod(temp.substr(pos + 1, temp.size() - pos));
				}
			}
		}
        fin.close();
    }
    {//site_bandwidth.csv  init id2n_s & site_bandwidth_vec
        string temp;
        fin.open(site_bandwidth_path, ios_base::in | ios_base::binary);
		getline(fin,temp);
		while (getline(fin,temp)) {
			if (temp.size() > 0) {
				int pos = temp.find(',');
				id2n_s.emplace_back(temp.substr(0, pos));
				site_bandwidth.emplace_back(stoi(temp.substr(pos + 1, temp.size() - pos)));
			}
		}
		fin.close();
		N_s = id2n_s.size();
    }
    {//demand.csv   init id2n_c & demand
        string temp;
        fin.open(demand_path, ios_base::in | ios_base::binary);
		fin >> temp;
		temp = temp.substr(16) + ',';
		string name_c;
		for (char &ch : temp) {
			if (ch == ',') {
				id2n_c.emplace_back(name_c);
				name_c.clear();
			}
			else {
				name_c += ch;
			}
		}
		N_c = id2n_c.size();

		N_t = 0;
		string last_time;
		while (getline(fin, temp)) {
			if (temp.size() >= 17) {
				string cur_time = temp.substr(0, 16);
				if (cur_time != last_time) {
					last_time = cur_time;
					demand_t_vec.emplace_back(Demand_t());
					N_t++;
				}

				temp = temp.substr(17, temp.size() - 18) + ',';
				int pos = temp.find(',');
				demand_t_vec[N_t - 1].stream_num++;
				demand_t_vec[N_t - 1].stream_name.emplace_back(temp.substr(0, pos));
				n_stream_set.emplace(temp.substr(0, pos));
				demand_t_vec[N_t - 1].demand.emplace_back(vector<int>(0));
				temp = temp.substr(pos + 1, temp.size() - pos);

				string band;
				for (char &ch : temp) {
					if (ch == ',') {
						demand_t_vec[N_t - 1].demand[demand_t_vec[N_t - 1].stream_num - 1].emplace_back(stoi(band));
						band.clear();
					}
					else {
						band += ch;
					}
				}
			}
		}
		fin.close();
    }
    {//id_c & id_s map string - id
		for (int id_c = 0; id_c < N_c; id_c++) {
			n2id_c[id2n_c[id_c]] = id_c; 
		}
		for (int id_s = 0; id_s < N_s; id_s++) {
			n2id_s[id2n_s[id_s]] = id_s; 
		}
	}
	{//qos.csv
        string temp;
        fin.open(qos_path, ios_base::in | ios_base::binary);
        fin >> temp;
		temp = temp.substr(10) + ',';
		string qos_s_name;
		vector<int> qos_s_id;//qos_id to id_s
		for (char &ch : temp) {
			if (ch == ',') {
				qos_s_id.emplace_back(n2id_c[qos_s_name]);
				qos_s_name.clear();
			}
			else {
				qos_s_name += ch;
			}
		}
		while (getline(fin, temp)) {
			int i = 0;
			if (temp.size() > 0) {
				int j = -1;
				temp += ',';
				string qos_str;
				for (char &ch : temp) {
					if (ch == ',') {
						if (j >= 0) {
							if (stoi(qos_str) < qos_constraint) {
								qos[i][qos_s_id[j]] = true;
							}
						}
						else {
							i = n2id_s[qos_str];
						}
						qos_str.clear();
						j++;
					}
					else {
						qos_str += ch;
					}
				}
			}
		}
		fin.close();
    }

    //connection map init
	vector<vector<int>> c_connected_to_s(N_s);
	for (int id_s = 0; id_s < N_s; id_s++) {
		for (int id_c = 0; id_c < N_c; id_c++) {
			if (qos[id_s][id_c]) {
				c_connected_to_s[id_s].emplace_back(id_c);
			}
		}
	}
	vector<vector<int>> s_connected_to_c(N_c);
	for (int id_c = 0; id_c < N_c; id_c++) {
		for (int id_s = 0; id_s < N_s; id_s++) {
			if (qos[id_s][id_c]) {
				s_connected_to_c[id_c].emplace_back(id_s);
			}
		}
	}
	// s  sort_vec init 
	vector<int> id_s_sort;
	for (int id_s = 0; id_s < N_s; id_s++) {
		if (c_connected_to_s[id_s].size() > 0) {
			id_s_sort.emplace_back(id_s);
		}
	}
	//t_sort init
	vector<int> t_sort;
	for (int t = 0; t < N_t; t++) {
		t_sort.emplace_back(t);
	}
    //free_num init 95% & cache_percent init
	vector<int> free_num(N_s, N_t / 20);//5%
	vector<vector<int>> cache_percent(N_t+1, vector<int>(N_s, 5));//5%

	vector<int> s_sort_p(N_s, 0);
	vector<int> best_s_sort_p;

START:	
	time_t  t_once_start = time(NULL);
	//srand(time(NULL));

    //used & free band
	vector<vector<int>> cache_band(N_t, vector<int>(N_s, 0));//t & id_s to cache_band  cache_band[t] = (cache_band[t-1] +  used_band[t-1]) * cache_percent
	vector<vector<int>> used_band(N_t, vector<int>(N_s, 0));//t & id_s to used_band   cache_band[t] + used_band[t] <= site_bandwidth
	vector<int> center_used_band(N_t, 0);//t & id_s to used_band


	vector<set<int>> tar_s_set(N_t+1);//1% x 20


    //initial destination
	for (int t = 0; t < N_t; t++) {
		Demand_t &demand_t = demand_t_vec[t];
		demand_t.destination.clear();
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			demand_t.destination.emplace_back(vector<int>(N_c, -1));
		}
	}


	//sort for burst
	sort(id_s_sort.begin(), id_s_sort.end(), [&](const int &a, const int &b){
		if (c_connected_to_s[a].size() == c_connected_to_s[b].size()) {
            return site_bandwidth[a] > site_bandwidth[b];
        }
        return c_connected_to_s[a].size() > c_connected_to_s[b].size();
	});

	vector<vector<int>> bursted(N_t, vector<int>(N_s, 0));
    //burst
	for (int t = 1; t < N_t; t++) {
		for (int &id_s : id_s_sort) {
			tar_s_set[t].emplace(id_s);
			cache_percent[t][id_s] = 1;
			if (tar_s_set[t].size() == 20) {
				break;
			}
		}
	}
	for (int &id_s : id_s_sort) {
		priority_queue <tuple<double, int>, vector<tuple<double, int>>, greater<tuple<double, int>>> burst_queue;//升序队列，小顶堆
		for (int t = 0; t < N_t; t++) {
            Demand_t &demand_t = demand_t_vec[t];
			double demand_t_c_sum = 0;
			for (int &id_c : c_connected_to_s[id_s]) {
				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.demand[stream_id][id_c] > 0 && demand_t.destination[stream_id][id_c] == -1) {
						demand_t_c_sum += (double)demand_t.demand[stream_id][id_c] / s_connected_to_c[id_c].size();
					}
				}
			}

			if (demand_t_c_sum > base_cost) {
				if (burst_queue.size() < free_num[id_s]) { 
					burst_queue.push(tuple<double, int>(demand_t_c_sum, t));
				}
				else if (demand_t_c_sum > get<0>(burst_queue.top())) {
					burst_queue.pop();
					burst_queue.push(tuple<double, int>(demand_t_c_sum, t));
				}
			}
        }


		while (!burst_queue.empty()) {
			int t = get<1>(burst_queue.top());
			bursted[t][id_s] = 1;
			if (t > 0 && tar_s_set[t].count(id_s) != 0) {
				cache_band[t][id_s] = (int)((cache_band[t-1][id_s] +  used_band[t-1][id_s]) * cache_percent[t][id_s] / 100);
			}
			else if (t > 0) {
				cache_band[t][id_s] = (int)((cache_band[t-1][id_s] +  used_band[t-1][id_s]) * cache_percent[t][id_s] / 100);
			}

			burst_queue.pop();
			Demand_t &demand_t = demand_t_vec[t];


			vector<int> sum_str(demand_t.stream_num, 0);
			for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
				for (int &id_c : c_connected_to_s[id_s]) {
					if (demand_t.destination[stream_id][id_c] == -1) {
						sum_str[stream_id] += demand_t.demand[stream_id][id_c];
					}
				}
			}
			
			vector<tuple<int, int, int, int>> small_demand_vec; 
			for (int &id_c : c_connected_to_s[id_s]) {
				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.demand[stream_id][id_c] > 0 && demand_t.destination[stream_id][id_c] == -1) {
						small_demand_vec.emplace_back(tuple<int, int, int, int>(sum_str[stream_id], demand_t.demand[stream_id][id_c], stream_id, id_c));
					}
				}
			}
			sort(small_demand_vec.begin(), small_demand_vec.end(), greater<tuple<int, int, int, int>>());//降序
			for (int i = 0; i < small_demand_vec.size(); i++) {
				int band = get<1>(small_demand_vec[i]);
				int stream_id = get<2>(small_demand_vec[i]);
				int id_c = get<3>(small_demand_vec[i]);

				int used = used_band[t][id_s];
				if (cache_band[t][id_s] == 0) {
					if (t > 0 && tar_s_set[t].count(id_s) == 0) {
						used += (int)(site_bandwidth[id_s] * 5 / 100);
					}
					else if (t > 0 && tar_s_set[t].count(id_s) != 0) {
						used += (int)(site_bandwidth[id_s] * 1 / 100);
					}
				}
				else {
					used += cache_band[t][id_s];
				}
				used += band;
				if (used <= site_bandwidth[id_s]) {
					demand_t.destination[stream_id][id_c] = id_s;
					used_band[t][id_s] += band;
				}
			}
			
		}
    }



    //used_t_set initial
	for (int t = 0; t < N_t; t++) {
		for (int &id_s : id_s_sort) {
			if (t > 0 && tar_s_set[t].count(id_s) == 0) {
				cache_band[t][id_s] = (int)((cache_band[t-1][id_s] + used_band[t-1][id_s]) * cache_percent[t][id_s] / 100);
			}
			else if (t > 0) {
				cache_band[t][id_s] = (int)((cache_band[t-1][id_s] +  used_band[t-1][id_s]) * cache_percent[t][id_s] / 100);
			}
		}
	}
    vector<Usedband_t_set> used_t_set(N_s);
	for (int &id_s : id_s_sort) {

		used_t_set[id_s].big_size = free_num[id_s];// N_t/20
		used_t_set[id_s].small_size = N_t - used_t_set[id_s].big_size;// N_t - N_t/20

		sort(t_sort.begin(), t_sort.end(), [&](const int &a, const int &b){
			return (used_band[a][id_s] + cache_band[a][id_s]) < (used_band[b][id_s] + cache_band[b][id_s]);
		});

		for (int t_i = 0; t_i < used_t_set[id_s].small_size; t_i++) {
			int &t = t_sort[t_i];
			used_t_set[id_s].small_set.emplace(used_band[t][id_s] + cache_band[t][id_s], t);
		}

		for (int t_i = N_t - 1; t_i >= used_t_set[id_s].small_size; t_i--) {
			int &t = t_sort[t_i];
			used_t_set[id_s].big_set.emplace(used_band[t][id_s] + cache_band[t][id_s], t);
		}
	}


    //center initial
	Center_site center_site(N_t, N_s);
	for (int t = 0; t < N_t; t++) {
		Demand_t &demand_t = demand_t_vec[t];
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			for (int id_c = 0; id_c < N_c; id_c++) {
				int id_s = demand_t.destination[stream_id][id_c];
				if (id_s != -1) {
					center_site.flow_c[t][id_s][stream_id].emplace(demand_t.demand[stream_id][id_c], id_c);
				}
			}
		}
		for (int &id_s : id_s_sort) {
			for (int stream_id = 0; stream_id < N_stream_MAX; stream_id++) {
				center_used_band[t] += center_site.get_max_flow(t, id_s, stream_id);
			}
		}
		center_site.set_emplace(pair<int, int>(center_used_band[t], t));
	}

    //sort for connect
	for (int id_c = 0; id_c < N_c; id_c++) {
		random_shuffle(s_connected_to_c[id_c].begin(), s_connected_to_c[id_c].end());
		sort(s_connected_to_c[id_c].begin(), s_connected_to_c[id_c].end(), [&](const int &a, const int &b){
			if (s_sort_p[a] != s_sort_p[b]) {
				return s_sort_p[a] > s_sort_p[b];
			}
            if (c_connected_to_s[a].size() == c_connected_to_s[b].size()) {
                return site_bandwidth[a] > site_bandwidth[b];
            }
            return c_connected_to_s[a].size() > c_connected_to_s[b].size();
		});
	}
    //connect
	for (int t = 0; t < N_t; t++) {

		if (t > 0) {
			vector<int> usedcache_s_vec(N_s,0);

			for (int &id_s : id_s_sort) {
				if (bursted[t-1][id_s] == 0) {
					usedcache_s_vec[id_s]=(cache_band[t-1][id_s] +  used_band[t-1][id_s]);
				}
			}
		}

		// if (t > 0) {
		// 	vector<int> add_s_vec;
		// 	vector<int> usedcache_s_vec(N_s,0);

		// 	for (int &id_s : id_s_sort) {
		// 		//if (bursted[t-1][id_s] == 0) {
		// 			add_s_vec.emplace_back(id_s);
		// 			usedcache_s_vec[id_s]=(cache_band[t-1][id_s] +  used_band[t-1][id_s]);
		// 		//}
		// 	}

		// 	sort(add_s_vec.begin(), add_s_vec.end(),[&](int a, int b){
		// 		return usedcache_s_vec[a] > usedcache_s_vec[b];
		// 	});

		// 	int pos=0;
		// 	while (tar_s_vec[t].size() < 20) {
		// 		tar_s_vec[t].emplace_back(add_s_vec[pos]);
		// 		pos++;
		// 	}
		// }

		for (int &id_s : id_s_sort) {
			if (t > 0) {
				cache_band[t][id_s] = (int)((cache_band[t-1][id_s] +  used_band[t-1][id_s]) * cache_percent[t][id_s] / 100);
			}
		}

		Demand_t &demand_t = demand_t_vec[t];//arrange one day demand

		vector<int> sum_str(demand_t.stream_num, 0);
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			for (int id_c = 0; id_c < N_c; id_c++) {
				if (demand_t.destination[stream_id][id_c] == -1) {
					sum_str[stream_id] += demand_t.demand[stream_id][id_c];
				}
			}
		}


		priority_queue <tuple<int, int, int, int>, vector<tuple<int, int, int, int>>, less<tuple<int, int, int, int>>> d_queue;//降序队列，大顶堆 
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			for (int id_c = 0; id_c < N_c; id_c++) {
				if (demand_t.demand[stream_id][id_c] > 0 && demand_t.destination[stream_id][id_c] == -1) {
					d_queue.push(tuple<int, int, int, int>(sum_str[stream_id], demand_t.demand[stream_id][id_c], stream_id, id_c));
				}
			}
		}
		while (!d_queue.empty()) {
			int band = get<1>(d_queue.top());
			int stream_id = get<2>(d_queue.top());
			int id_c = get<3>(d_queue.top());
			d_queue.pop();

			int tar_id_s = -1;
			double d_cost_min = DBL_MAX;

			for (int &id_s : s_connected_to_c[id_c]) {
				int used = used_band[t][id_s];
				used += cache_band[t][id_s];
				used += band;
				if (used <= site_bandwidth[id_s]) {
					if (bursted[t][id_s] == 1) {//bursted[t][id_s] == 1
						tar_id_s = id_s;
						break;
					}
				}
			}

			if (tar_id_s == -1) {
				for (int &id_s : s_connected_to_c[id_c]) {

					int used = used_band[t][id_s];

					used += cache_band[t][id_s];
					
					used += band;
					if (used <= site_bandwidth[id_s]) {

						double delta_cost = 0;
						if (used <= used_t_set[id_s].get_score_band() || used <= base_cost || used_t_set[id_s].get_state(pair<int, int>(used_band[t][id_s], t)) == 1) { // || used_t_set[id_s].get_state(pair<int, int>(used_band[t][id_s], t)) == 1
							delta_cost = 0;
						}
						else {
							double origin_cost = cul_cost(used_t_set[id_s].get_score_band(), site_bandwidth[id_s]);
							double new_cost = cul_cost(used, site_bandwidth[id_s]);
							delta_cost = new_cost - origin_cost;
						}


						if (delta_cost == 0) {
							tar_id_s = id_s;
							break;
						}

						if (d_cost_min > delta_cost) {
							d_cost_min = delta_cost;
							tar_id_s = id_s;
						}
					}
				}
			}


			if (tar_id_s == -1) {
				cout<<"err0"<<endl;
				exit(1);
			}

			add(cache_percent, demand_t_vec, used_t_set, center_site, used_band, cache_band, center_used_band, t, stream_id, id_c, tar_id_s);
		}
	}


    int now_cost = calculate_cost(used_t_set, center_site);
	cout<<"cost = "<<now_cost<<endl;

	if (min_cost > now_cost) {//save best ans
		min_cost = now_cost;
		//save best state
		for (int t = 0; t < N_t; t++) {
			Demand_t &demand_t = demand_t_vec[t];
			demand_t.ans = demand_t.destination;
		}
		//save best state
		//best_s_sort_p = s_sort_p;
	}
	else {
		//s_sort_p = best_s_sort_p;
	}

	//
	for (int &id_s : id_s_sort) {
		s_sort_p[id_s] += used_t_set[id_s].get_score_band();
		s_sort_p[id_s] /= 4;
	}
	//



    t_once_max = fmax(t_once_max, time(NULL) - t_once_start);
	if (time(NULL) - start_time + t_once_max < MAX_TIME) {
		//goto START;
	}
	cout<<"time = "<<(time(NULL) - start_time)<<" s"<<endl;



//

OUTPUT:
    {//solution.txt
		fout.open(output_path, ios_base::out | ios_base::binary);

		for (int t = 0; t < N_t; t++) {

			if (t > 0) {
				if (tar_s_set[t].size()!=20) {
					cout<<"err!"<<endl;
					exit(0);
				}
				string str = "";
				for(int item : tar_s_set[t]) {
					str += id2n_s[item] + ",";
				}
				str = str.replace(str.length()-1, 1, "");
				fout << str << endl;
			}

			Demand_t &demand_t = demand_t_vec[t];
			for (int id_c = 0; id_c < N_c; id_c++) {
				fout << id2n_c[id_c] << ":";
		
				bool flag = true;
				vector<vector<int>> rx_stream_id_vec(N_s);//id_s to rx_stream_id_vec

				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.ans[stream_id][id_c] != -1) {
						rx_stream_id_vec[demand_t.ans[stream_id][id_c]].emplace_back(stream_id);
					}
				}

				for (int id_s = 0; id_s < N_s; id_s++) {
					if (rx_stream_id_vec[id_s].size() > 0) {
						if (flag) {
							flag = false;
						}
						else {
							fout << "," ;
						}
						fout << "<" << id2n_s[id_s];
						for (int &rx_stream_id : rx_stream_id_vec[id_s]) {
							fout << "," << demand_t.stream_name[rx_stream_id];
						}
						fout << ">";
					}
				}
				fout << endl;
			}
		}
		fout.close();
	}
    
    return 0;
}