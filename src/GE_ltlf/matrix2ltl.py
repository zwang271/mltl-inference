import torch
import math
import json
import numpy as np
import random
import time
import os
from copy import deepcopy
from multiprocessing import Pool
import argparse

def show_depend(model_file):
    parameter=torch.load(model_file)
    torch.set_printoptions(threshold=10000, precision=1, sci_mode=False)
    # print(parameter['predict.weight'])
    weight=parameter['predict.weight'].numpy()

    # for row in weight:
    #     scale=0
    #     for i in row:
    #         if math.fabs(i)>scale:
    #             scale=math.fabs(i)
        # row/=scale

        # print(row)
    # print(parameter['predict.bias'].numpy())
    return weight,parameter['predict.bias'].numpy()

def get_all_or(or_voc):
    # print('orvoc',or_voc)
    if len(or_voc)==1:
        return or_voc[0]
    if len(or_voc)==2:
        return ('||',or_voc[0],or_voc[1])
    else:
        return ('||',or_voc[0],get_all_or(or_voc[1:]))

def get_all_and(and_voc):

    if len(and_voc)==1:
        return and_voc[0]
    if len(and_voc)==2:
        # print('get all and', and_voc[0],)
        if and_voc[0]=='':
            return and_voc[1]
        if and_voc[1]=='':
            return and_voc[0]
        return ('&&',and_voc[0],and_voc[1])
    else:
        if and_voc[0]=='':
            return get_all_and(and_voc[1:])
        return ('&&',and_voc[0],get_all_and(and_voc[1:]))

def get_not(voc_tup,subformulae):
    if voc_tup[0]>0:
        return subformulae[voc_tup[1]]
    else:
        return ('!',subformulae[voc_tup[1]])

def no_depend(row,formula_len,vocab_len,tgt):
    if tgt<formula_len+vocab_len and tgt<=row:
        return True
    if tgt>=formula_len+vocab_len and tgt<row+formula_len+vocab_len:
        return True
    return False

#'model/blocks/blocks_d0/E2_8_f4_e300.model'
# 'blocks/blocks_d0/E2_8.json'
def loss_for_and(w1,w2,bias): # f_i = a & b
    return 1/(1+((w1-1)**2+(w2-1)**2+(bias+1)**2)**0.5)

def loss_for_one(w1,bias): # next
    return 1/(1+((1-w1)**2)**0.5)

def loss_for_not(w1,bias):
    return 1/(1+((w1+1)**2+(bias-1)**2)**0.5)


def loss_for_U(w1,w2,w3,bias):
    return 1/(1+((w1-2)**2+(w2-1)**2+(w3-1)**2+(bias+1)**2)**0.5)



def get_ltl_from_list(t_list,vocab,formula_len,vocab_len):
    t_list.sort(key=lambda x:x[0],reverse=True)
    # t_list=[a[1:] for a in t_list]
    subformulae=['']*formula_len
    for i in range(vocab_len):
        subformulae.append(vocab[i])
    subformulae+=['']*formula_len
    for i in range(vocab_len):
        subformulae.append(vocab[i])
    row_idx=formula_len-1
    row_idx_map={}
    multiple_cnt=0
    for row_idx in range(len(t_list)):
    # while row_idx>=0:
        c_turple=t_list[row_idx][1:]
        row_idx=t_list[row_idx][0]
        if row_idx not in row_idx_map.keys():
            row_idx_map[row_idx] = 1
        else:
            multiple_cnt = 1
        try:
            if isinstance(c_turple,int):
                subformulae[row_idx]=subformulae[c_turple]
            else:
                subformulae[row_idx]=tuple([subformulae[i] if isinstance(i,int) else i for i in c_turple]) # 运算符->运算符，数字->子公式
            subformulae[row_idx+formula_len+vocab_len]=subformulae[row_idx]
        except:
            print('interpret error')
            print(t_list)
            print(len(t_list))
            print(row_idx)
            print(formula_len-1-row_idx)
            exit(0)
        row_idx-=1
    # print(subformulae[0])
    return subformulae[0],multiple_cnt

def get_notturple(row_idx,i,global_best,loss,formula_len):
    if i>=formula_len:# atomic
        return [([(row_idx,'!',i)],loss)]
    else:
        return [(k[0]+[(row_idx,'!',i)],loss*k[1]) for k in global_best[i]]

def get_andturple(row_idx,i,j,global_best,loss,formula_len):
    if i>=formula_len:# atomic
        if j>=formula_len:
            return [([(row_idx,'&&',i,j)],loss)]
        else:
            return [(k[0]+[(row_idx,'&&',i,j)],loss*k[1]) for k in global_best[j]]
    else:
        if j>=formula_len:
            return [(k[0]+[(row_idx,'&&',i,j)],loss*k[1]) for k in global_best[i]]
        else:
            return [(k1[0]+k2[0]+[(row_idx,'&&',i,j)], loss*k1[1]*k2[1]) for k1 in global_best[i] for k2 in global_best[j]]

def get_Xturple(row_idx,i,global_best,loss,formula_len,vocab_len):
    if i>=formula_len*2+vocab_len:# atomic
        return [([(row_idx,'X',i)],loss)]
    else:
        return [(k[0]+[(row_idx,'X',i)],loss*k[1]) for k in global_best[i-formula_len-vocab_len]]

def get_untilturple(row_idx,i,j,global_best,loss,formula_len):
    if i>=formula_len:# atomic
        if j>=formula_len:
            return [([(row_idx,'U',i,j)],loss)]
        else:
            return [(k[0]+[(row_idx,'U',i,j)],loss*k[1]) for k in global_best[j]]
    else:
        if j>=formula_len:
            return [(k[0]+[(row_idx,'U',i,j)],loss*k[1]) for k in global_best[i]]
        else:
            return [(k1[0]+k2[0]+[(row_idx,'U',i,j)], loss*k1[1]*k2[1]) for k1 in global_best[i] for k2 in global_best[j]]

def matrix2ltl(model_name,test_file_name,top_num):
    weight,bias=show_depend(model_name)
    np.set_printoptions(suppress=True,precision=2,linewidth=400)

    with open(test_file_name,'r') as f:
        vocab=json.load(f)['vocab']+['true']
    formula_len=len(weight)
    vocab_len=len(vocab)

    l_cof=1

    # print('matrix:')
    # for j in ['sub%d'%i for i in range(formula_len)]+vocab+['Xsub%d'%i for i in range(formula_len)]+['X'+i for i in vocab]:
    #     print(j,end='\t')
    #     if len(j)<=2:
    #         print(end='\t')
    #
    # print('')
    # for i in range(2*(vocab_len+formula_len)):
    #     print('%4d\t'%i,end='')
    # print('')
    # for i in weight:
    #     for j in i:
    #         print("%.2f"%j,end='\t')
    #     print('')
    # print('bias',bias)


    # print('ori subformulae',subformulae)
    row_idx=len(weight)-1
    global_best=[0]*formula_len

    while row_idx>=0:
        turple2loss=[] #([chooselist],loss)
        row_weight=weight[row_idx]
        total_weight=sum(abs(weight[row_idx]))
        for i in range(row_idx+1,len(row_weight)):
            # 依赖单个的有， f=a, f=!a,f=Fa,f=Ga,f=Na
            if i>=formula_len+vocab_len and i<=row_idx+formula_len+vocab_len: # X父公式不考虑
                continue
            if i<formula_len+vocab_len:
                turple2loss+=get_notturple(row_idx,i,global_best,l_cof*loss_for_not(row_weight[i], bias[row_idx]),formula_len)
                # turple2loss.append((k[0]+[(row_idx,'!',i)],k[1]*l_cof*loss_for_not(row_weight[i], bias[row_idx])) )# f=!a
                for j in range(row_idx + 1, formula_len+vocab_len):
                    if j > i:
                        turple2loss+=get_andturple(row_idx,i,j,global_best,l_cof*loss_for_and(row_weight[i], row_weight[j], bias[row_idx]),formula_len)
                    turple2loss+=get_untilturple(row_idx,i,j,global_best,l_cof*loss_for_U(row_weight[j], row_weight[i],row_weight[row_idx + formula_len + vocab_len],bias[row_idx]),formula_len)
            else:
                turple2loss+=get_Xturple(row_idx,i,global_best,l_cof*loss_for_one(row_weight[i], bias[row_idx]),formula_len,vocab_len)

        turple2loss.sort(key=lambda x:x[1],reverse=True)

        turple2loss=turple2loss[:top_num]
        # print(turple2loss)
        global_best[row_idx]=turple2loss
        # print('subformula %d'%row_idx, subformulae[row_idx])
        row_idx -= 1

    ret_formulae=[]
    errcnt=0

    for i in global_best[0]:
        # print(i)
        a,b=get_ltl_from_list(i[0],vocab,formula_len,vocab_len)
        errcnt+=b
        ret_formulae.append(a)
    # print('ret_formulae',ret_formulae)
    # a=input()
    return ret_formulae,errcnt


class LTLf():

    def __init__(self, vocab, LTLf_tree):
        self.vocab = vocab
        self.LTLf_tree = LTLf_tree
        self.cache = {}

    def _checkLTL(self, f, t, trace, vocab, c=None, v=False, orif=None):
        """ Checks satisfaction of a LTL formula on an execution trace

            NOTES:
            * This works by using the semantics of LTL and forward progression through recursion
            * Note that this does NOT require using any off-the-shelf planner

            ARGUMENTS:
                f       - an LTL formula (must be in TREE format using nested tuples
                        if you are using LTL dict, then use ltl['str_tree'])
                t       - time stamp where formula f is evaluated
                trace   - execution trace (a dict containing:
                            trace['name']:    trace name (have to be unique if calling from a set of traces)
                            trace['trace']:   execution trace (in propositions format)
                            trace['plan']:    plan that generated the trace (unneeded)
                vocab   - vocabulary of propositions
                c       - cache for checking LTL on subtrees
                v       - verbosity

            OUTPUT:
                satisfaction  - true/false indicating ltl satisfaction on the given trace
        """
        if orif == None:
            orif = f
        if v:
            print('\nCurrent t = ' + str(t))
            print('Current f =', f)

        ###################################################

        # Check if first operator is a proposition
        if type(f) is str:
            if f in vocab:
                return f in trace['trace'][t]
            if f == 'true':
                return True
            if f=='false':
                return False

        # Check if sub-tree info is available in the cache
        key = (f, t, trace['name'])
        if c is not None:
            if key in c:
                if v: print('Found subtree history')
                return c[key]

        # Check for standard logic operators
        # if len(f)==0:
        # 	print('f', f, 't', t, 'trace', trace, 'vocab', vocab, 'c', c, 'v', v)
        if f[0] in ['not', '!']:
            value = not self._checkLTL(f[1], t, trace, vocab, c, v, orif)
        elif f[0] in ['and', '&', '&&']:
            value = all((self._checkLTL(f[i], t, trace, vocab, c, v, orif) for i in range(1, len(f))))
        elif f[0] in ['or', '|', '||']:
            value = any((self._checkLTL(f[i], t, trace, vocab, c, v, orif) for i in range(1, len(f))))
        elif f[0] in ['imp', '->']:
            value = not (self._checkLTL(f[1], t, trace, vocab, c, v, orif)) or self._checkLTL(f[2], t, trace, vocab, c,
                                                                                              v, orif)

        # Check if t is at final time step
        elif t == len(trace['trace']) - 1:
            # Confirm what your interpretation for this should be.
            if f[0] in ['G', 'F']:
                value = self._checkLTL(f[1], t, trace, vocab, c, v,
                                       orif)  # Confirm what your interpretation here should be
            elif f[0] == 'U':
                value = self._checkLTL(f[2], t, trace, vocab, c, v, orif)
            elif f[0] == 'W':  # weak-until
                value = self._checkLTL(f[2], t, trace, vocab, c, v, orif) or self._checkLTL(f[1], t, trace, vocab, c, v,
                                                                                            orif)
            elif f[0] == 'R':  # release (weak by default)
                value = self._checkLTL(f[2], t, trace, vocab, c, v, orif)
            elif f[0] == 'X':
                value = False
            elif f[0] == 'N':
                value = True
            else:
                # Does not exist in vocab, nor any of operators
                print('f', f, 't', t, 'trace', trace, 'vocab', vocab, 'c', c, 'v', v)
                sys.exit('LTL check - something wrong 1')

        else:
            # Forward progression rules
            if f[0] == 'X' or f[0] == 'N':
                value = self._checkLTL(f[1], t + 1, trace, vocab, c, v, orif)
            elif f[0] == 'G':
                value = self._checkLTL(f[1], t, trace, vocab, c, v, orif) and self._checkLTL(('G', f[1]), t + 1, trace,
                                                                                             vocab, c, v, orif)
            elif f[0] == 'F':
                value = self._checkLTL(f[1], t, trace, vocab, c, v, orif) or self._checkLTL(('F', f[1]), t + 1, trace,
                                                                                            vocab, c, v, orif)
            elif f[0] == 'U':
                # Basically enforces f[1] has to occur for f[1] U f[2] to be valid.
                if t == 0:
                    if not self._checkLTL(f[1], t, trace, vocab, c, v, orif) and not self._checkLTL(f[2], t, trace,
                                                                                                    vocab, c, v,
                                                                                                    orif):  # if f[2] is ture at time 0,then it is true
                        value = False
                    else:
                        value = self._checkLTL(f[2], t, trace, vocab, c, v, orif) or (
                                    self._checkLTL(f[1], t, trace, vocab, c, v) and self._checkLTL(('U', f[1], f[2]),
                                                                                                   t + 1, trace, vocab,
                                                                                                   c, v, orif))
                else:
                    value = self._checkLTL(f[2], t, trace, vocab, c, v, orif) or (
                                self._checkLTL(f[1], t, trace, vocab, c, v) and self._checkLTL(('U', f[1], f[2]), t + 1,
                                                                                               trace, vocab, c, v,
                                                                                               orif))

            elif f[0] == 'W':  # weak-until
                value = self._checkLTL(f[2], t, trace, vocab, c, v, orif) or (
                            self._checkLTL(f[1], t, trace, vocab, c, v, orif) and self._checkLTL(('W', f[1], f[2]),
                                                                                                 t + 1, trace, vocab, c,
                                                                                                 v, orif))
            elif f[0] == 'R':  # release (weak by default)
                value = self._checkLTL(f[2], t, trace, vocab, c, v, orif) and (
                            self._checkLTL(f[1], t, trace, vocab, c, v, orif) or self._checkLTL(('R', f[1], f[2]),
                                                                                                t + 1, trace, vocab, c,
                                                                                                v, orif))
            else:
                # Does not exist in vocab, nor any of operators
                print('f', f, 't', t, 'trace', trace, 'vocab', vocab, 'c', c, 'v', v, ' orif', orif)
                sys.exit('LTL check - something wrong 2 ' + f[0])

        if v: print('Returned: ' + str(value))

        # Save result
        if c is not None and type(c) is dict:
            key = (f, t, trace['name'])
            c[key] = value  # append

        return value

    def pathCheck(self, trace, trace_name):

        trace_dir = {'name': trace_name, 'trace': tuple(trace)}
        return self._checkLTL(self.LTLf_tree, 0, trace_dir, self.vocab, self.cache)

    def evaluate(self, cluster1, cluster2):
        # print(self.LTLf_tree)
        check_pos_mark = []
        for i in range(len(cluster1)):
            st = self.pathCheck(cluster1[i], 'pos' + str(i))
            check_pos_mark.append(st)

        check_neg_mark = []
        for i in range(len(cluster2)):
            st = self.pathCheck(cluster2[i], 'neg' + str(i))
            check_neg_mark.append(st)
        return check_pos_mark, check_neg_mark

# 替换运算符得到一些公式
# & | ! U R F G X N


def get_best_ltlfs(train_dic,ltlfs):
    best_ltlfs=[]
    best_score_train=0
    # print('related_ltlfs',len(related_ltlfs))
    for ltlf_tree in ltlfs:
        ltlf=LTLf(train_dic['vocab'],ltlf_tree)
        check_pos_mark, check_neg_mark = ltlf.evaluate(train_dic['traces_pos'], train_dic['traces_neg'])
        cur_score= (sum(check_pos_mark)+len(train_dic['traces_neg'])-sum(check_neg_mark))/(len(train_dic['traces_pos'])+len(train_dic['traces_neg']))
        if cur_score>best_score_train:
            best_score_train=cur_score
            best_ltlfs=[ltlf_tree]
        elif cur_score==best_score_train:
            best_ltlfs.append(ltlf_tree)
    # print('best_score_train',best_score_train)
    return best_ltlfs




def test_matrix(model_name, train_file_name, test_file_name, top_num=100):
    # print('one start')
    # 'model/blocks/blocks_d0/E2_8_f4_e300.model'
    # 'blocks/blocks_d0/E2_8.json'
    # print('testing',train_file_name)
    with open(test_file_name,'r') as f:
        E_T_dic=json.load(f)
    with open(train_file_name,'r') as f:
        E_dic=json.load(f)

    int_time=time.time()
    ltlfs,errcnt=matrix2ltl(model_name, test_file_name,top_num)
    int_time=time.time()-int_time

    # print('target ltl:',E_T_dic["ltlftree"])

    rw_time=time.time()
    best_ltltree=get_best_ltlfs(E_dic,ltlfs)
    rw_time=time.time()-rw_time
    # ltlf_tree= best_ltltree[0]
    # ltlf = LTLf(E_T_dic["vocab"], ltlf_tree)
    ltlf_trees = best_ltltree
    ltlfs = [LTLf(E_T_dic["vocab"], ltlf_tree) for ltlf_tree in ltlf_trees]
    ltlf = ltlfs[0] # evaluate the first one

    try:
        check_pos_mark, check_neg_mark = ltlf.evaluate(E_T_dic['traces_pos'], E_T_dic['traces_neg'])
    except:
        print('error')
        return (-1,-1,-1,int_time,rw_time)
    correct=sum(check_pos_mark)+(len(E_T_dic['traces_neg'])-sum(check_neg_mark))

    TP=sum(check_pos_mark)
    FN=len(E_T_dic['traces_pos'])-TP
    FP=sum(check_neg_mark)
    total=len(E_T_dic['traces_pos'])+len(E_T_dic['traces_neg'])
    # print('correct',correct,'TP,FP,FN',(TP,FP,FN),'total',len(E_T_dic['traces_pos']+E_T_dic['traces_neg']))
    # a = input()
    # print('TP',TP,'FP',FP,'FN',FN)

    if TP==0:
        return (correct/total,0,0,int_time,rw_time)
    return (correct/total,TP/(TP+FP),TP/(TP+FN),int_time,rw_time,E_T_dic["ltlftree"],ltlf_trees)  # acc,pre,rec


def get_net_performance(result_dic):
    # i[0] 目标，i[1]预测
    TP=sum([1 if i[0]==1 and i[1]==1 else 0 for i in result_dic['test_result']]) #真阳个数
    FP=sum([1 if i[0]==0 and i[1]==1 else 0 for i in result_dic['test_result']])  #假阳个数
    FN=sum([1 if i[0]==1 and i[1]==0 else 0 for i in result_dic['test_result']]) #假阴
    TN = sum([1 if i[0] == 0 and i[1] == 0 else 0 for i in result_dic['test_result']])  # 真阴
    # if result_dic['correct']!=TP+TN:
    #     print(TP,TN,FP,FN,result_dic['correct'])
    # assert result_dic['correct']==TP+TN
    assert result_dic['total']==TP+FP+FN+TN
    if TP+FP==0:
        return result_dic['correct']/result_dic['total'],0,TP/(TP+FN),float(result_dic['time'])
    return result_dic['correct']/result_dic['total'],TP/(TP+FP),TP/(TP+FN),float(result_dic['time'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Main script for parallel')
    parser.add_argument('-train_file', type=str, required=True,  help='training set file name')
    parser.add_argument('-test_file', type=str, required=True,  help='test set file name')
    parser.add_argument('-save_model', type=str, required=True, help='saved model name')
    parser.add_argument('-top_num', type=int, required=True, help='saved model name')
    args = parser.parse_args()

    # save_model='mindata_f3_d0-2_0.model'
    # train_file='mindata_f3_d0/E2_0.json'
    # test_file = 'mindata_f3_d0/ET2_0.json'
    # top_num=100
    res=test_matrix(args.save_model, args.train_file, args.test_file, args.top_num)
    acc, pre, rec, int_time, refine_time= res[0],res[1],res[2],res[3],res[4]
    print('acc:%f, pre:%f, rec:%f, interpretation time:%f'%(acc,pre,rec,int_time+refine_time))
    print('target ltl:', res[-2])
    print('learned ltl:',res[-1])

#
# def main(args):
#     #formula_lens = [5,10,15]
#     formula_lens = [5,10,15]
#     formula_lens = [3,6,9,12,15]
#     epoch_num = 200000
#     tag = args.tag
#     domains = ['mindata_f%d' % i for i in formula_lens]
#     inss = [2]
#     cases = [i for i in range(0, 50)]
#     top_num = args.top_num
#     noise_rate= args.noise
#     possibility = 0.4
#     job_size = 35
#     fix_len=True
#     fix_formula_len=args.fix_formula_len
#     # get formula and test
#     formula_result = []
#     pool = Pool(processes=job_size)
#     fcnt = -1
#     for i in range(len(domains)):
#         domain = domains[i]
#         fcnt += 1
#         floder = 'tag_%d/%s_d%d' % (tag, domain,noise_rate)
#         model_floder = 'model1/' + floder
#         cnt=0
#         for ins in inss:
#             for case in cases:
#                 test_file = 'data/%s_d%d/ET%d_%d.json' % (domain,noise_rate, ins, case)
#                 train_file = 'data/%s_d%d/E%d_%d.json' % (domain,noise_rate, ins, case)
#                 if fix_len:
#                     save_model = model_floder + '/E%d_%d_domain%s_netlen%d_epoch%d_tag%d.model' % (
#                         ins, case, domain,fix_formula_len, epoch_num, tag)
#                 else:
#                     save_model = model_floder + '/E%d_%d_flen%d_epoch%d_tag%d.model' % (
#                         ins, case, formula_lens[fcnt], epoch_num, tag)
#                 # print('ins%d case%d netresult:%d/%d' % (ins,case,datas[cnt]['correct'], datas[cnt]['total']))
#                 # test_matrix(save_model, train_file, test_file, top_num)
#
#                 cnt+=1
#                 formula_result.append(
#                     pool.apply_async(test_matrix,
#                                      (save_model, train_file, test_file, top_num)))
#     pool.close()
#     pool.join()
#
#     ret = [x.get() for x in formula_result]  # 返回每个用的时间
#
#     fcnt = -1
#     for i in range(len(domains)):
#         fname = 'model1/tag_%d/result_domain%s_netlen%d_epoch%d_tag%d.json' % (tag, domains[i],fix_formula_len, epoch_num, tag)
#         domain = domains[i]
#         with open(fname, 'r') as f:
#             datas = json.load(f)
#         cnt = 0
#         fcnt += 1
#         floder = 'tag_%d/%s_d%d' % (tag, domain,noise_rate)
#         res_floder = 'result'
#         model_floder = 'model1/' + floder
#         # print('domain: ',domain)
#         for ins in inss:
#             a_acc, a_pre, a_rec, a_f1, int_time, rw_time = 0, 0, 0, 0, 0, 0
#             net_acc, net_pre, net_rec, net_f1, net_time = 0, 0, 0, 0, 0
#             casenum = len(cases)
#             for case in cases:
#                 formula_result = ret[len(inss) * len(cases) * i + cnt]
#                 if len(formula_result)<5:
#                     print(formula_result)
#                 # acc,pre,rec,itime,rtime
#                 acc = formula_result[0]
#                 pre = formula_result[1]
#                 rec = formula_result[2]
#                 itime = formula_result[3]
#                 rtime = formula_result[4]
#
#                 if pre+rec==0:
#                     f1=0
#                 else:
#                     f1 = 2 * pre * rec / (pre + rec)
#                 if acc == -1:
#                     casenum -= 1
#                     continue
#                 a_acc += acc
#                 a_pre += pre
#                 a_rec += rec
#                 a_f1 += f1
#                 int_time += itime
#                 rw_time += rtime
#
#                 acc, pre, rec, t = get_net_performance(datas[cnt])
#                 if pre+rec==0:
#                     f1=0
#                 else:
#                     f1 = 2 * pre * rec / (pre + rec)
#                 net_acc += acc
#                 net_pre += pre
#                 net_rec += rec
#                 net_f1 += f1
#                 net_time += t
#                 cnt += 1
#
#             print('%f\t%f\t' % (net_acc / casenum, a_acc / casenum), end='')
#             print('%f\t%f\t' % (net_pre / casenum, a_pre / casenum), end='')
#             print('%f\t%f\t' % (net_rec / casenum, a_rec / casenum), end='')
#             print('%f\t%f\t' % (net_f1 / casenum, a_f1 / casenum), end='')
#             print('%f\t%f\t%f' % (net_time / casenum, int_time / casenum, rw_time / casenum))
#
#             # a=input('waiting')
#
# if __name__ == '__main__':
#     parser = argparse.ArgumentParser(description='Main script for parallel')
#     parser.add_argument('-tag', type=int, required=True,  help='domains')
#     parser.add_argument('-top_num', type=int, required=True,  help='top k in interpretation')
#     parser.add_argument('-noise', type=int, required=True, help='formula len of network')
#     parser.add_argument('-fix_formula_len', type=int, required=False, default=3, help='epoch number')
#
#
#     #python matrix2ltl_templatev2L2.py -tag 1000 -max_num 1 -noise 0 -fix_formula_len 3
#     args = parser.parse_args()
#     main(args)