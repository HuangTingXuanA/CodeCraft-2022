import datetime
import os
from itertools import combinations
from functools import reduce
import sys
from random import shuffle
import numpy as np
import math
# 参数设置
base_cost = 1000
# 带宽参数设置
bandLimit = [18888,200000]
# 需求参数设置
# 大颗粒占比
possibility = 0.7
# 大颗粒设置
demandLimitBig = [1000,10000]
# 小颗粒设置
demandLimitSmall = [0,700]


record = None
record_band = None
record_demand = None
server_num, client_num, time_len, stream_num = 100, 10, 100, 10
pressure = 0.9
qos_lim = 400
qos = None
dist_matrix = None

def ask(msg: str, default: int):
    inputed = input(msg)
    if inputed.strip():
        return int(inputed.strip())
    return default

def read_input():
    global server_num, client_num, time_len, stream_num
    server_num = ask('input server number (default 100):', 100)
    client_num = ask('input client number (default 10):', 10)
    time_len = ask('input time length (default 100):', 100)
    stream_num= ask('input stream number (default 100):', 100)

def distribute_server():
    global record_band, record_demand,qos_lim, qos
    # qos矩阵
    qos = np.ceil(np.random.normal(400, 50, size=(server_num, client_num))).astype('int32')
    # 参数带宽
    record_band = np.random.randint(bandLimit[0], bandLimit[1] ,size=server_num).astype('int32')

    # 看需要多少需求需求
    each_demand = np.ceil(20000).astype('int32')
    record_demand = np.zeros((time_len*stream_num, client_num))
    for t_idx in range(time_len*stream_num):
        record_demandTemp = np.zeros(client_num)
        for i in range(client_num):
            temp = np.random.rand(1)
            if temp > 1 - possibility:
                record_demandTemp[i] = np.random.randint(demandLimitBig[0], demandLimitBig[1], size=1)
            else:
                record_demandTemp[i] = np.random.randint(demandLimitSmall[0],demandLimitSmall[1], size=1)
        record_demand[t_idx] = record_demandTemp
def gen_client_name(n: int):
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    out = list(combinations(alphabet, 2))
    out = [ reduce(lambda x, y: x+y, each) for each in out ]
    out += [ i for i in alphabet ]
    shuffle(out)
    out = out[:n]
    out.sort()
    return out

def gen_server_name(n: int):
    cand = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789'
    out = list(combinations(cand, 2))
    out = [ reduce(lambda x, y: x+y, each) for each in out ]
    shuffle(out)
    out = out[:n]
    out.sort()
    return out

def gen_stream_name(n: int):
    cand = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789'
    out = list(combinations(cand, 2))
    out = [ reduce(lambda x, y: x+y, each) for each in out ]
    shuffle(out)
    out = out[:n]
    out.sort()
    return out

def output(path: str):
    curr_time = datetime.datetime(2021, 11, 1, 0, 0)
    time_sep = datetime.timedelta(minutes=5)
    sname = gen_server_name(server_num)
    cname = gen_client_name(client_num)
    stream_name = gen_client_name(stream_num)
    with open(os.path.join(path, 'demand.csv'), 'w') as f:
        f.write('mtime,' + 'stream_id'+','+','.join(cname) +'\r'+ '\n')
        for t_idx in range(time_len):
            time_str = curr_time.strftime("%Y-%m-%dT%H:%M")
            for t_ in range(stream_num):
                c_demand = np.ceil(record_demand[t_idx*stream_num+t_]).astype('int32')
                c_demand = [str(i) for i in c_demand]
                f.write(time_str + ',' +stream_name[t_]+','+ ','.join(c_demand) +'\r'+ '\n')
            curr_time += time_sep
    with open(os.path.join(path, 'site_bandwidth.csv'), 'w') as f:
        f.write('site_name,bandwidth\r\n')
        for s_idx in range(server_num):
            # 服务器的带宽值
            bd_used = record_band[s_idx]
            bd_upper = np.ceil(bd_used).astype('int32')
            f.write(f'{sname[s_idx]},{bd_upper}\r\n')
    with open(os.path.join(path, 'qos.csv'), 'w') as f:
        f.write('site_name,' + ','.join(cname) +'\r'+ '\n')
        for s_idx in range(server_num):
            s = sname[s_idx]
            qos_list = qos[s_idx]
            qos_list = [ str(i) for i in qos_list ]
            qos_str = ','.join(qos_list)
            f.write(f'{s},{qos_str}\r\n')
    with open(os.path.join(path, 'config.ini'), 'w') as f:
        f.write('[config]\r\n')
        f.write(f'qos_constraint={qos_lim}\r\n')
        f.write(f'base_cosst={base_cost}\r\n')

if __name__ == '__main__':
    read_input()
    record = np.zeros((time_len, server_num, client_num), dtype=np.int32)
    distribute_server()
    if len(sys.argv) == 1:
        output('pressure')
    else:
        try: os.mkdir(sys.argv[1])
        except: pass
        output(sys.argv[1])
    
