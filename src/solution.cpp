#include "solution.h"
#define INF 0x3f3f3f3f

solution::solution()
{   
    edgeNodes = new EdgeNode[N];
    customNodes = new CustomNode[M];
}

solution* solution::instance = nullptr;
solution* solution::getInstance()
{
    if(!instance) instance = new solution();
    return instance;
}

solution::~solution() {
    
}

void solution::init_nodes(){
    string temp;
    int nodeNum = 0;
    
    //config文件读取
    fin.open(file_qosConfig, ios_base::in | ios_base::binary);
    if(!fin) {cerr << "未能打开txt文件" << endl; exit(-1);}
    getline(fin, temp);
    getline(fin, temp);
    temp = temp.substr(15);
    maxQos = stoi(temp);
    fin.close();

    //bandWidth文件读取
    fin.open(file_bandWidth, ios_base::in | ios_base::binary);
    vector<string> item_bandWidth;
    while(getline(fin, temp)) item_bandWidth.push_back(temp);
    realN = item_bandWidth.size() - 1;
    for(int i = 0; i < item_bandWidth.size(); i++){
        string site_name, bandwith_value;
        istringstream sstream(item_bandWidth[i]);
        getline(sstream, site_name, ',');
        getline(sstream, bandwith_value, '\r');
        if(i == 0) continue;
        else{
            edgeNodes[i].name = site_name;
            edgeNodes[i].bandwidth = stoi(bandwith_value);
            edgeOfId.insert({site_name, i});
        }
    }
    fin.close();

    //demand文件读取node名称
    fin.open(file_demand, ios_base::in | ios_base::binary);
    getline(fin, temp);
    istringstream sstream(temp);
    getline(sstream, temp, ',');
    while(getline(sstream, temp, ',')){
        realM++;
        int pos = temp.find("\r");
        if(pos != string::npos) temp.replace(pos, temp.length()-pos, "");
        customNodes[realM].name = temp;
        customOfId.insert({temp, realM});
    }
    fin.close();

    //qos读取
    fin.open(file_qos, ios_base::in | ios_base::binary);
    vector<string> item_qos;
    while(getline(fin, temp)) item_qos.push_back(temp);
    nodeNum = count(item_qos[0].begin(), item_qos[0].end(), ',');
    int id_customs[M] = {0};
    for(int i = 0; i < item_qos.size(); i++){
        string str, value;
        istringstream sstream(item_qos[i]);
        if(i == 0){
            getline(sstream, str, ',');
            for(int j = 1; j <= nodeNum; j++){
                getline(sstream, str, ',');
                int pos = str.find("\r");
                if(pos != string::npos) str.replace(pos, str.length()-pos, "");
                id_customs[j] = customOfId[str];
            }
        }
        else{
            getline(sstream, str, ',');
            for(int j = 1; j <= nodeNum; j++){
                getline(sstream, value, ',');
                int pos = value.find("\r");
                if(pos != string::npos) value.replace(pos, value.length()-pos, "");
                if(stoi(value) < maxQos) customNodes[id_customs[j]].qos.insert(str);
            }
        }
    }
    fin.close();

    //读取时间T行
    fin.open(file_demand, ios_base::in | ios_base::binary);
    getline(fin, temp);
    vector<string> item_demand;
    while(getline(fin, temp)) item_demand.push_back(temp);
    T = item_demand.size();
    for(int i = 0; i < T; i++){
        istringstream ssstream(item_demand[i]);
        getline(ssstream, temp, ',');
        int j = 0;
        while(getline(ssstream, temp, ',')){
            j++;
            int pos = temp.find("\r");
            if(pos != string::npos) temp.replace(pos, temp.length()-pos, "");
            demandInfos[i].push_back({customNodes[j], stoi(temp)});
        }
    }
    fin.close();
}

/**
deque<pair<CustomNode, int>> solution::get_next_demand(){
    string mtime, value;
    if(!fin.is_open()){
        fin.open(file_demand, ios_base::in | ios_base::binary);
        getline(fin, value);
    }
    deque<pair<CustomNode, int>> data;
    getline(fin, mtime);
    istringstream sstream(mtime);
    getline(sstream, value, ',');
    int i = 0;
    while(getline(sstream, value, ',')){
        i++;
        int pos = value.find("\r");
        if(pos != string::npos) value.replace(pos, value.length()-pos, "");
        data.push_back({customNodes[i], stoi(value)});
    }
    return data;
}
*/

auto queueCmp = [](const pair<CustomNode, int> &a, const pair<CustomNode, int> &b){
    return a.second < b.second;
};
void solution::handle(deque<pair<CustomNode, int>>& demandInfo){
    //大带宽需求优先队列
    priority_queue<pair<CustomNode, int>, deque<pair<CustomNode, int>>, decltype(queueCmp)> queue(queueCmp);
    for(auto item : demandInfo) queue.push({item.first, item.second});
    memset(h, -1, sizeof h);
    w[N*M]={0}, f[N*M]={0}, e[N*M]={0}, ne[N*M]={0}, idx=0;
    //构建S - E层
    for(auto it : freeBandwidth){
        auto findEdgeItem = find_if(edgeVarId.begin(), edgeVarId.end(), [it](const map<int, int>::value_type item){
                    return item.second == it.first;
                });
        if(findEdgeItem == edgeVarId.end()) puts("edgeVarId error");
        int nowHave = it.second.second-((it.second.first-1)*edgeNodes[(*findEdgeItem).first].bandwidth);
        if(nowHave > 0) add(0, it.first, 0, nowHave);
    }
    //哑变量Id构建图
    while(queue.size()){
        auto t = queue.top();
        queue.pop();
        //按free次数重排满足qos的edgeNode
        vector<string> qos;
        for(auto item = t.first.qos.begin(); item != t.first.qos.end(); item++)
            qos.push_back(*item);
        sort(qos.begin(), qos.end(), [this](const string &a, const string &b){
            int aVarId = edgeVarId[edgeOfId[a]], bVarId = edgeVarId[edgeOfId[b]];
            return freeBandwidth[aVarId].first < freeBandwidth[bVarId].first;
        });
        //构建E - C1层，费用很重要
        for(auto it = qos.begin(); it != qos.end(); it++){
            int eVarId = edgeVarId[edgeOfId[*it]];
            int cost = (freeBandwidth[eVarId].second - edgeNodes[edgeOfId[*it]].bandwidth*(freeBandwidth[eVarId].first-1))%10;
            if(cost >= 0) add(eVarId, customVarId[customOfId[t.first.name]], cost, t.second);
        }
        //构建C1 - C2层
        add(customVarId[customOfId[t.first.name]], customVarId[customOfId[t.first.name]]+realM, 0, t.second);
        //构建C2 - T层
        add(customVarId[customOfId[t.first.name]]+realM, 2*realM+realN+1, 0, t.second);
    }
    //最小费用最大流
    maxflow(0, 2*realM+realN+1);

    //收集答案
    delete[] answers;
    answers = new Answer[M];
    map<int, int> edgeVarIdUsage;
    for(int u = 1; u <= realM; u++){
        for(int i = h[u]; ~i; i = ne[i]){
            int j = e[i];
            if(j == u+realM) continue;
            if(f[i] > 0){
                auto findEdgeItem = find_if(edgeVarId.begin(), edgeVarId.end(), [j](const map<int, int>::value_type item){
                    return item.second == j;
                });
                if(findEdgeItem == edgeVarId.end()) puts("edgeVarId error");
                answers[u].mp[edgeNodes[(*findEdgeItem).first].name] = f[i];
                edgeVarIdUsage.insert({j, f[i]});
            }
        }
    }
    for(auto item : edgeVarIdUsage)
        if(freeBandwidth[item.first].first > 0) freeBandwidth[item.first].first--, freeBandwidth[item.first].second -= edgeNodes[item.first-2*realM].bandwidth;
}

//a到b的费用c、容量d
void solution::add(int a, int b, int c, int d){
    e[idx] = b, w[idx] = c, f[idx] = d, ne[idx] = h[a], h[a] = idx++;
    e[idx] = a, w[idx] = -c, f[idx] = 0, ne[idx] = h[b], h[b] = idx++;
}

int solution::maxflow(int s, int t){
    int res = 0;
    int minflow;
    while(spfa(s, t)){
        minflow = INF+1;
        for(int i = pre[t]; ~i; i = pre[e[i^1]])
            if(f[i] < minflow) minflow = f[i];
        for(int i = pre[t]; ~i; i = pre[e[i^1]])
            f[i] -= minflow, f[i^1] += minflow;
        res += minflow;
    }
    return res;
}

bool solution::spfa(int s, int t){
    for(int i = 0; i < 2*N+M; i++) dist[i]=INF, st[i]=false, pre[i]=-1;
    dist[s] = 0;
    queue<int> q;
    st[s] = true;
    q.push(s);
    while(q.size()){
        int x = q.front();
        q.pop();
        st[x] = false;
        for(int i = h[x]; ~i; i = ne[i]){
            int j = e[i];
            if(f[i] && dist[j] > dist[x] + w[i]){
                dist[j] = dist[x] + w[i];
                pre[j] = i;
                if(!st[j]) st[j] = true, q.push(j);
            }
        }
    }
    return pre[t] == -1?false:true;
}

void solution::slove(){
    groupCnt = floor(T*0.04);
    beginFlag = true;
    //哑变量Id分配，先客户节点再边缘节点，key:真实Id，value:哑变量Id
    for(int i = 1; i <= 2*realM+realN; i++){
        if(i <= 2*realM) customVarId.insert({i, i});
        else edgeVarId.insert({i-2*realM, i});
    }

    //分组任务预处理 first:edge哑变量ID    second:pair<组内可用次数，剩余可用带宽>
    for(int i = 2*realM+1; i <= 2*realM+realN; i++)
        freeBandwidth[i] = {groupCnt, edgeNodes[i-2*realM].bandwidth*groupCnt};

    //读取每一行需求信息
    for(int i = 0; i < T; i++){
        handle(demandInfos[i]);
        printf_result(beginFlag);
        beginFlag = false;
    }
}

void solution::printf_result(bool beginFlag){
    if(!fout.is_open()) fout.open(file_output, ios_base::out | ios_base::binary);
    for(int i = 1; i <= realM; i++){
        int cnt = answers[i].mp.size();
        if(beginFlag && i == 1) fout << customNodes[i].name << ":";
        else fout << "\n" << customNodes[i].name << ":";
        if(cnt == 0) continue;
        string tmpStr;
        //通过迭代器取出map的first和second
        for(auto it : answers[i].mp)
            tmpStr += "<"+it.first+","+to_string(it.second)+">,";
        tmpStr.replace(tmpStr.length()-1, 1, "");
        fout << tmpStr;
    }
    fout.flush();
}
