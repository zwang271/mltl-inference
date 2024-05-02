import torch
import torch.nn.functional as F
import math
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter
import os
import json
from copy import deepcopy
from multiprocessing import Pool
import argparse
import time

os.environ["CUDA_VISIBLE_DEVICES"]='0'
device = torch.device("cpu")
# device = torch.device(
#     "cuda") if torch.cuda.is_available() else torch.device("cpu")



class CenteredLayer(torch.nn.Module):
    def __init__(self, **kwargs):
        super(CenteredLayer, self).__init__(**kwargs)
    def forward(self, x):
        x[x<0]=x[x<0]*0.1
        x[x>1]=x[x>1]*0.1+0.9
        return x


class Regular_loss(torch.nn.Module):
    def __init__(self):
        super(Regular_loss, self).__init__()

    def forward(self, weight,bias,formula_len,vocab_len,cof_L1,cof_range):
        ret_x=[0]*len(weight)
        for i in range(len(weight)):
            regular_l1=torch.sum(torch.abs(weight[i]))# L1正则
            regular_range = torch.sum(torch.relu(weight[i][:formula_len+vocab_len]-2)) #C <2
            regular_range += torch.sum(torch.relu(-weight[i][:formula_len+vocab_len]-1)) #C>-1
            regular_range += torch.sum(torch.relu(weight[i][formula_len+vocab_len:]-1)) #A<1
            regular_range += torch.sum(torch.relu(-weight[i][formula_len + vocab_len:])) #A>0
            regular_range += torch.relu(-bias[i]-1)+torch.relu(bias[i]-1)

            ret_x[i] = regular_l1*cof_L1 + regular_range*cof_range

        return ret_x

class Net(torch.nn.Module):
    def __init__(self,formula_len,vocab_len):
        super(Net,self).__init__()
        self.formula_len=formula_len
        self.vocab_len=vocab_len
        self.predict=torch.nn.Linear((formula_len+vocab_len)*2,formula_len,bias=True)

        new_state_dict=self.predict.state_dict()

        for i in range(formula_len):
            for j in range(i):
                new_state_dict['weight'][i][j]=0  # C[i][j] = 0 where i>=j
                new_state_dict['weight'][i][j+formula_len+vocab_len] = 0 # A[i][j]=0 where i>j
            new_state_dict['weight'][i][i]=0
            for j in range(i,formula_len+vocab_len): # A>0
                new_state_dict['weight'][i][j + formula_len + vocab_len] = math.fabs(new_state_dict['weight'][i][j + formula_len + vocab_len])

        self.predict.load_state_dict(new_state_dict,strict=False)
        self.myrelu = CenteredLayer()
        # print('parameter',self.predict.state_dict())


    def forward(self,x): # x : (*,formula_len+vocab_len)
        layers_size=len(x)+self.formula_len
        end_x = torch.FloatTensor([[0] * (self.formula_len+self.vocab_len)]).to(device)
        for i in range(layers_size):
            next_x = torch.cat((x[1:],end_x))    #  next_x : (*,formula_len+vocab_len)
            input_x = torch.cat((x,next_x),1)    #  input_x : (*,(formula_len+vocab_len)*2)
            nx=self.predict(input_x)  # nx : (*,formula_len)
            nx=self.myrelu(nx)
            x= torch.cat((nx,x[:,self.formula_len:]),1)  #  x: (*,formula_len+vocab_len)
        x=x-0.5
        x=torch.sigmoid(x*5)
        return x





# torch.save(net.state_dict(), save_name)
class Trace_Dataset(Dataset):
    def __init__(self, raw_data):
        self.item = raw_data
        for i in range(len(self.item)):
            self.item[i][0]=torch.FloatTensor(self.item[i][0]).to(device)

    def __getitem__(self, idx):
        return self.item[idx][0],self.item[idx][1]

    def __len__(self):
        return len(self.item)

def get_data(fname,formula_len,word2idx=-1,vocab=-1):
    with open(fname,'r') as f:
        data=json.load(f)

    if word2idx==-1:
        word2idx={}
        vocab=data['vocab']+['true']
        cnt=0
        for v in vocab:
            word2idx[v]=cnt
            cnt+=1

    datas=[]
    # print('pos traces:',len(data['traces_pos']),'neg traces',len(data['traces_neg']))
    cnt=0
    for trace in data['traces_pos']:
        cnt+=1
        # print(trace)
        idx_trace=[]
        for state in trace:
            idx_state=[-1]*(len(vocab))
            for v in state:
                idx_state[word2idx[v]]=1
            idx_state[word2idx['true']]=1

            idx_state=[0]*formula_len+idx_state
            idx_trace.append(idx_state)
        datas.append([idx_trace,1])
    # print('pos_trace',len(datas))
    cnt=0
    for trace in data['traces_neg']:
        cnt+=1
        idx_trace=[]
        for state in trace:
            idx_state=[-1]*(len(vocab))
            for v in state:
                idx_state[word2idx[v]]=1
            idx_state[word2idx['true']] = 1
            idx_state=[0]*formula_len+idx_state
            idx_trace.append(idx_state)
        datas.append([idx_trace, 0])

    # print('pos_trace+neg_traces', len(datas))
    # for data in datas:
    #     print(data)
    # print(datas)
    dataset=Trace_Dataset(datas)
    return dataset,word2idx,vocab


def train(train_file,formula_len,save_name,epoch_num,log_file,ins=0,case=0,learn_rate=2e-4,l1_cof=0.2):
    stime=time.time()
    train_dataset,word2idx,vocab=get_data(train_file,formula_len)
    # test_dataset, _, _ = get_data('blocks/blocks_d0/E2_0.json', formula_len, word2idx, vocab)
    torch.set_printoptions(threshold=10000, precision=1, sci_mode=False)
    min_loss = 1e6
    train_dataloader=DataLoader(train_dataset,batch_size=1,shuffle=True)
    loss_func = torch.nn.MSELoss()
    regular_loss_func = Regular_loss()
    zero_grad_matrix = [
        [0 if j <= i or (j >= formula_len + len(vocab) and j < formula_len + len(vocab) + i) else 1 for j in
         range((formula_len + len(vocab)) * 2)] for i in range(formula_len)]
    zero_grad_matrix = torch.FloatTensor(zero_grad_matrix)

    writer = SummaryWriter(log_file)

    model = Net(formula_len,len(vocab))
    model.to(device)
    model.predict.weight.register_hook(lambda grad: grad.mul_(zero_grad_matrix))

    optimizer = torch.optim.AdamW(model.parameters(), lr=learn_rate)
    cnt=0
    batch_size=10
    for epoch in range(epoch_num):
        epoch_loss=0
        for batch_data in train_dataloader:
            data=batch_data[0] #每个例子过的层数不一样。。所以就一个一个跑了
            prediction = model(data[0])
            label = torch.FloatTensor([batch_data[1]]).to(device)
            loss = loss_func(prediction[0][0], label)
            epoch_loss+= loss.detach().cpu().numpy()
            # loss/=len(train_dataset)
            loss.backward()
            cnt += 1
            # print('grad1',model.predict.weight.grad)
            if cnt%batch_size==0:
                regular_loss = regular_loss_func(model.predict.weight, model.predict.bias, formula_len, len(vocab),
                                                 l1_cof, 10)
                for r in range(len(regular_loss)):
                    regular_loss[r].backward()
                # print('grad2', model.predict.weight.grad)
                optimizer.step()
                optimizer.zero_grad()
                cnt=0
        if epoch_loss<min_loss:
            min_loss=epoch_loss
            torch.save(model.state_dict(), save_name)

        writer.add_scalar("Loss/train-E%d_%d-netlen%d"%(ins,case,formula_len), epoch_loss, epoch)
        if epoch % 10 == 0:
            print(
                'train_file:%s \t epoch:%d \t loss:%f \t %d' % (train_file, epoch, epoch_loss, len(train_dataset)))

    writer.close()
    return time.time()-stime


def test(test_file,formula_len,word2idx,vocab,model_file,save_file):
    test_dataset,_,_=get_data(test_file,formula_len,word2idx,vocab)
    model=Net(formula_len,len(vocab))
    model.load_state_dict(torch.load(model_file))
    torch.set_printoptions(threshold=10000,precision=1,sci_mode=False)
    # print((model.state_dict()['predict.weight']).cpu())
    # print((model.state_dict()['predict.bias']).cpu())

    total_cnt=0
    correct_cnt=0
    test_result=[]
    with torch.no_grad():
        with open(save_file,'w') as f:
            for idx in range(len(test_dataset)):
                data = test_dataset[idx]
                prediction = model(data[0].to(device))
                f.write('predict case %d , predict result/given label= %f/%d\n' % (idx, prediction[0][0], data[1]))
                if (data[1]-0.5)* (prediction[0][0]-0.5) >0:
                    correct_cnt+=1
                total_cnt+=1
                test_result.append([1 if data[1] >0.5 else 0, 1 if prediction[0][0]>0.5 else 0])
                # print('predict case %d , predict result/given label= %f/%d' % (idx, prediction[0][0], data[1]))
    print(test_file,' %d/%d'%(correct_cnt,total_cnt))
    return correct_cnt,total_cnt,test_result


# a single example for one dataset
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Main script for parallel')
    parser.add_argument('-train_file', type=str, required=True,  help='training set file name')
    parser.add_argument('-test_file', type=str, required=True,  help='test set file name')
    parser.add_argument('-save_model', type=str, required=True, help='saved model name')
    parser.add_argument('-log_file', type=str, required=True,  help='log file')
    parser.add_argument('-res_file', type=str, required=True, help='result file')
    parser.add_argument('-network_len', type=int, required=False, default=3, help='the K of the network')
    parser.add_argument('-epoch_num', type=int, required=False, default=400, help='number of epoch')
    parser.add_argument('-lr', type=float, required=False, default=1e-4, help='learn rate')
    parser.add_argument('-l1', type=float, required=False, default=0.1, help='l1 cof')
    args = parser.parse_args()

    # train_file='mindata_f3_d0/E2_0.json'
    # test_file = 'mindata_f3_d0/ET2_0.json'
    # save_model='mindata_f3_d0-2_0.model'
    # log_file='mindata_f3_d0-2_0.log'
    # res_file='mindata_f3_d0-2_0.txt'
    # network_len = 3
    # epoch_num = 400
    # lr=1e-4
    # l1=0.1
    train(train_file=args.train_file, 
          formula_len=args.network_len, 
          save_name=args.save_model, 
          epoch_num=args.epoch_num, 
          log_file=args.log_file,
          learn_rate=args.lr,
          l1_cof=args.l1)
    
    _, word2idx, vocab = get_data(args.train_file, args.network_len)
    test(test_file=args.test_file,
         formula_len=args.network_len,
         word2idx=word2idx,vocab=vocab,
         model_file=args.save_model,
         save_file=args.res_file)



# uncommond the following code to run training on multiple datasets
# def main(args):
#
#
#     total_sample_num = args.epoch
#     tag = args.tag
#     repeat_times=1
#     noise_rate=args.noise_num
#     fix_len=True
#     fix_lens=[int(i) for i in args.netlen.split()]
#     result = []
#
#     domains=args.domains.split()
#     instace_num=[int(i) for i in args.inss.split()]
#     case_num = [i for i in range(0,args.casenum)]
#     pool = Pool(processes=args.jobsize)
#     fcnt=0
#     for domain in domains:
#         floder = 'data/%s_d%d' % (domain,noise_rate)
#         res_floder = 'result'
#         model_floder = 'model1/tag%d/%s_d%d' % (tag, domain, noise_rate)
#         if not os.path.exists(model_floder):
#             os.makedirs(model_floder)
#         if not os.path.exists(res_floder):
#             os.makedirs(res_floder)
#         epoch_nums=[total_sample_num//i for i in [500,100,500,1000]]
#         # epoch_nums = [total_sample_num // i for i in [500, 1000]]
#         for ins in instace_num:
#             for case in case_num:
#                 train_file = floder + '/E%d_%d.json' % (ins, case)
#                 save_model = model_floder + '/E%d_%d_domain%s_netlen%d_epoch%d_tag%d.model' % (
#                 ins, case, domain,fix_lens[fcnt], total_sample_num, tag)
#                 log_file = res_floder + '/%s_tag%d' % (domains[fcnt],tag)
#                 # train(train_file, formula_lens[fcnt], save_model, epoch_nums[ins], log_file,repeat_times)
#                 # if fix_len:
#                 result.append(pool.apply_async(train, (
#                 train_file, fix_lens[fcnt], save_model, epoch_nums[ins], log_file,ins,case, args.lr,args.l1)))
#                 # else:
#                 #     result.append(pool.apply_async(train, (train_file, formula_lens[fcnt], save_model, epoch_nums[ins], log_file,ins,case,repeat_times)))
#         fcnt += 1
#     print('total',len(result))
#     pool.close()
#     pool.join()
#     ret = [x.get() for x in result] #返回每个用的时间
#
#
#
#     cnt = 0
#     fcnt = 0
#     for domain in domains:
#         print('%s result---------' % (domain))
#         result = []
#         floder = 'data/%s_d%d' % (domain,noise_rate)
#         res_floder = 'result'
#         model_floder = 'model1/tag%d/%s_d%d'%(tag,domain,noise_rate)
#         for ins in instace_num:
#             for case in case_num:
#                 test_file=floder+'/ET%d_%d.json'%(ins,case)
#                 train_file=floder+'/E%d_%d.json'%(ins,case)
#                 log_file = res_floder + '/E%d_%d_err2.txt' % (ins, case)
#                 save_model = model_floder + '/E%d_%d_domain%s_netlen%d_epoch%d_tag%d.model' % (
#                     ins, case, domain, fix_lens[fcnt], total_sample_num, tag)
#                 _, word2idx, vocab = get_data(train_file, fix_lens[fcnt])
#                 correct, total, test_result = test(test_file, fix_lens[fcnt], word2idx, vocab, save_model,
#                                                    log_file)
#                 # save_model=model_floder+'/E%d_%d_flen%d_epoch%d_tag%d.model'%(ins,case,formula_lens[fcnt],total_sample_num,tag)
#                 result.append({'correct': correct, 'total': total,'test_result':test_result, 'time': ret[cnt]})
#                 cnt+=1
#         with open('model1/tag%d/result_domain%s_netlen%d_epoch%d_tag%d.json'%(tag,domain,fix_lens[fcnt],total_sample_num,tag),'w') as f:
#             json.dump(result,f,indent=2)
#         fcnt += 1
#
#
#
# if __name__ == '__main__':
#     parser = argparse.ArgumentParser(description='Main script for parallel')
#     parser.add_argument('-domains', type=str, required=True,  help='domains')
#     parser.add_argument('-netlen', type=str, required=True,  help='formula len of network')
#     parser.add_argument('-tag', type=int, required=True, help='formula len of network')
#     parser.add_argument('-epoch', type=int, required=False, default=200000, help='epoch number')
#     parser.add_argument('-inss', type=str, required=False, default='0 1 2 3', help='instances')
#     parser.add_argument('-casenum', type=int, required=False, default=50, help='number of cases')
#     parser.add_argument('-lr', type=float, required=False, default=1e-4, help='learn rate')
#     parser.add_argument('-l1', type=float, required=False, default=0.2, help='l1 cof')
#     parser.add_argument('-jobsize', type=int, required=False, default=35, help='threads number')
#     parser.add_argument('-noise_num', type=int, required=False, default=0, help='noise number')
#
#     #python learn_ltl_matrix.py -domains='mindata_f6 mindata_f6' -netlen='5 10' -tag=205 -epoch=200000 -inss='2' -casenum=50 -lr=1e-4 -jobsize=35 -noise_num=0 -l1=0.2
#     args = parser.parse_args()
#     main(args)
