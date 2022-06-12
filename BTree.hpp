#ifndef BTREE_HPP
#define BTREE_HPP
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <cstring>
#include <filesystem>

#include <fstream>
#include <string>
#include <utility>
#include <stack>
#include <queue>

#define BPT_DEBUG

#ifdef BPT_DEBUG
#include <iostream>
using std::cout;
using std::endl;
#endif

namespace bpt_util{

using std::string;
using std::fstream;
using std::ifstream;
using std::ofstream;

template<class T, int info_len = 2>
class MemoryRiver {
private:
    fstream file;
    string file_name;
    static const int sizeofT = sizeof(T);
    static const int headPtr = info_len*sizeof(int);
    static const int dataStart = (info_len+1)*sizeof(int);
    inline std::ios::pos_type ptrPos(int ind){//data index is 0-based
        return dataStart+(sizeofT+sizeof(int))*(ind);
    }
    inline std::ios::pos_type datPos(int ind){
        return dataStart+(sizeofT+sizeof(int))*(ind)+sizeof(int);
    }
public:
    MemoryRiver() = default;

    MemoryRiver(const string& file_name) : file_name(file_name) {}

    void initialize(string FN = "") {
        if (FN != "") file_name = FN;
        file.open(file_name, std::ios::out|std::ios::binary);
        int tmp = 0;
        for (int i = 0; i < info_len; ++i)
            file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.close();
    }

    //读出第n个int的值赋给tmp，1_base
    void get_info(int &tmp, int n) {
        if (n > info_len) return;
        file.open(file_name, std::ios::in|std::ios::binary);
        file.seekg((n-1)*sizeof(int));
        file.read(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.close();
    }

    //将tmp写入第n个int的位置，1_base
    void write_info(int tmp, int n) {
        if (n > info_len) return;
        file.open(file_name, std::ios::in|std::ios::out|std::ios::binary);
        file.seekp((n-1)*sizeof(int));
        file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.close();
    }

    //在文件合适位置写入类对象t，并返回写入的位置索引index
    //位置索引意味着当输入正确的位置索引index，在以下三个函数中都能顺利的找到目标对象进行操作
    //位置索引index可以取为对象写入的起始位置
    int write(T &t) {
        //std::cout<<"River"<<file_name<<"write:"<<std::endl;
        int toWritePos;
        file.open(file_name, std::ios::in|std::ios::out|std::ios::binary);
        file.seekg(headPtr);
        file.read(reinterpret_cast<char *>(&toWritePos), sizeof(int));
        file.seekg(ptrPos(toWritePos));
        if(file.peek()!=EOF){
            int nPos;//next Pos available for writing
            file.read(reinterpret_cast<char *>(&nPos), sizeof(int));
            file.flush();
            file.seekp(headPtr);
            file.write(reinterpret_cast<char *>(&nPos), sizeof(int));
        }
        else{
            int nPos=toWritePos+1;//next Pos available for writing
            file.flush();
            file.seekp(headPtr);
            file.seekg(headPtr);
            //file.seekg(headPtr);
            file.write(reinterpret_cast<char *>(&nPos), sizeof(int));
            int tmp=0xdeadbeef;
            file.seekp(ptrPos(toWritePos));
            file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
            file.flush();
        }
        //int tmp=datPos(toWritePos);
        //file.seekg(datPos(toWritePos));
        file.seekp(datPos(toWritePos));
        file.write(reinterpret_cast<char *>(&t) , sizeofT);
        file.flush();
        file.close();
        return toWritePos;
    }

    //用t的值更新位置索引index对应的对象，保证调用的index都是由write函数产生
    void update(T &t, const int index) {
        file.open(file_name, std::ios::in|std::ios::out|std::ios::binary);
        file.seekp(datPos(index));
        file.write(reinterpret_cast<char *>(&t) , sizeofT);
        file.close();
    }

    //读出位置索引index对应的T对象的值并赋值给t，保证调用的index都是由write函数产生
    void read(T &t, const int index) {
        file.open(file_name, std::ios::in|std::ios::binary);
        file.seekg(datPos(index));
        file.read(reinterpret_cast<char *>(&t) , sizeofT);
        file.close();
    }

    //删除位置索引index对应的对象(不涉及空间回收时，可忽略此函数)，保证调用的index都是由write函数产生
    void Delete(int index) {
        file.open(file_name, std::ios::in|std::ios::out|std::ios::binary);
        int nPos;
        file.seekg(headPtr);
        file.read(reinterpret_cast<char *>(&nPos), sizeof(int));
        file.seekp(ptrPos(index));
        file.write(reinterpret_cast<char *>(&nPos), sizeof(int));//现在删除的项的指针=headPtr
        file.seekp(headPtr);
        file.write(reinterpret_cast<char *>(&index), sizeof(int));//headPtr指向现在删除的项
        file.close();
    }
};

}


namespace sjtu {
    constexpr int max(int x,int y){return x>y?x:y;}
    template <class Key, class Value>
    class BTree {
    public:
        class iterator;
    //using std::pair;
    private:
        // Your private members go here

        static constexpr int M=max((4096+sizeof(Key)-2*sizeof(int)-sizeof(bool))/(sizeof(int)+sizeof(Key)),5),HALFM=(M%2)?(M/2+1):(M/2),HALFMP1=((M+1)%2)?((M+1)/2+1):((M+1)/2);//HALFM:ceil(M/2) HALFMP1:ceil((M+1)/2) TODO
        
        //int size=0;//TODO:Move to HDD
        struct node{
            bool isleaf=false;
            int size=0;
            int child[M]={0};
            //int parent=0;
            int pre=-1;
            Key keys[M-1];
            node(){
                child[M-1]=-1;
            }
        };
        
        bpt_util::MemoryRiver<node> *nodes;bpt_util::MemoryRiver<Value> *values;
        inline int getRoot(){
            int tmp;
            nodes->get_info(tmp,1);
            return tmp;
        }
        inline void setRoot(int rt){
            nodes->write_info(rt,1);
        }
        void init(){
            size=0;
            setRoot(0);
        }
        struct sizeWrapper{
            BTree *tree;
            sizeWrapper(BTree *t):tree(t){}
            operator int(){
                int siz;
                tree->nodes->get_info(siz,2);
                return siz;
            }
            int operator++(int){
                int siz;
                tree->nodes->get_info(siz,2);
                siz++;
                tree->nodes->write_info(siz,2);
                return siz-1;
            }
            int operator--(int){
                int siz;
                tree->nodes->get_info(siz,2);
                siz--;
                tree->nodes->write_info(siz,2);
                return siz+1;
            }
            int operator=(int x){
                tree->nodes->write_info(x,2);
                return x;
            }
        }size=(this);
        //iterator lower_bound(int nod,const Key &k);


    public:
#ifdef BPT_DEBUG
        void examineNode(int p,std::queue<int> *q=nullptr){
            cout<<"Node "<<p<<endl;
            node n;
            nodes->read(n,p);
            cout<<"node size:"<<n.size<<endl;
            cout<<"keys:"<<endl;
            for(int i=0;i<n.size;i++)cout<<n.keys[i]<<',';
            cout<<endl;
            if(!n.isleaf){
                cout<<"children:"<<endl;
                for(int i=0;i<=n.size;i++)cout<<n.child[i]<<',';
                cout<<endl;
                if(q){
                    for(int i=0;i<=n.size;i++)q->push(n.child[i]);
                }
            }
            else{
                cout<<"leaf children:"<<endl;
                for(int i=0;i<n.size;i++){
                    Value v;
                    values->read(v,n.child[i]);
                    cout<<n.keys[i]<<":"<<n.child[i]<<":"<<v<<',';
                }
                cout<<endl;
                cout<<"pre:"<<n.pre<<",next:"<<n.child[M-1]<<endl;
            }
            cout<<"-----------"<<endl;
        }
        void bfs_traverse(){
            cout<<"size:"<<size<<endl;
            if(!size)return;
            int rt=getRoot();
            cout<<"root"<<rt<<endl;
            std::queue<int> que;
            que.push(rt);
            while(!que.empty()){
                int nd=que.front();
                que.pop();
                examineNode(nd,&que);
            }
            cout<<"============"<<endl;
            cout<<endl;
        }
#endif
        BTree() {
            nodes=new bpt_util::MemoryRiver<node>("bptree.nodes.dat");
            values=new bpt_util::MemoryRiver<Value>("bptree.value.dat");
            if(!std::filesystem::exists("bptree.nodes.dat")){
                values->initialize();
                nodes->initialize();
                init();
            }
        }

        BTree(const char *fname) {
            std::string fn(fname);
            std::string nfn=fn+".nodes.dat",vfn=fn+".value.dat";
            nodes=new bpt_util::MemoryRiver<node>(nfn.c_str());
            values=new bpt_util::MemoryRiver<Value>(vfn.c_str());
            if(!std::filesystem::exists(nfn))
            {
                values->initialize();
                nodes->initialize();
                init();
            }
        }

        ~BTree() {
            delete nodes;
            delete values;
        }

        // Clear the BTree
        void clear() {
            values->initialize();
            nodes->initialize();
            init();
        }

        bool insert(const Key &key, const Value &value) {
            std::stack<std::pair<int,int>> path;//first:节点在nodes中的index second:节点在父节点中的child编号
            if(!size){
                node nod;
                nod.isleaf=true;
                Value tvalue=value;
                int valpos=values->write(tvalue);
                nod.keys[0]=key;
                nod.child[0]=valpos;
                nod.size=1;
                int rt=nodes->write(nod);
                setRoot(rt);
                size=1;
                return true;
            }
            int ptr=getRoot();
            node currentNode;
            path.push(std::make_pair(ptr,0));
            nodes->read(currentNode,ptr);
            while(!currentNode.isleaf){
                int nextBranch=currentNode.size;//branch for next node to visit
                for(int i=0;i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){nextBranch=i;break;}
                }
                path.push(std::make_pair(currentNode.child[nextBranch],nextBranch));
                nodes->read(currentNode,currentNode.child[nextBranch]);
            }
            int findPos=-1;
            for(int i=0;i<currentNode.size;i++){//TODO
                if(currentNode.keys[i]==key){findPos=i;break;}
            }
            if(findPos!=-1)return false;
            if(currentNode.size<M-1){
                findPos=currentNode.size;
                for(int i=0;i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){findPos=i;break;}
                }
                
                for(int i=currentNode.size;i>=findPos+1;i--){
                    currentNode.keys[i]=currentNode.keys[i-1];
                    currentNode.child[i]=currentNode.child[i-1];
                }
                currentNode.size++;
                //memmove(currentNode.keys+findPos+1,currentNode.keys+findPos,(currentNode.size-findPos)*sizeof(Key));
                //memmove(currentNode.child+findPos+1,currentNode.child+findPos,(currentNode.size-findPos-1)*sizeof(int));
                currentNode.keys[findPos]=key;
                Value tvalue=value;
                currentNode.child[findPos]=values->write(tvalue);
                nodes->update(currentNode,path.top().first);
                size++;
                return true;
            }
            int currentno=path.top().first;
            int currentchildno=path.top().second;
            path.pop();
            node split1,split2;
            split1.isleaf=split2.isleaf=true;
            Key *tmpkey=new Key[M+1];
            int *tmpchild=new int[M+2];
            findPos=-1;
            {
                int i;
                for(i=0;i<M-1&&currentNode.keys[i]<key;i++){
                    tmpkey[i]=currentNode.keys[i];
                    tmpchild[i]=currentNode.child[i];
                }
                tmpkey[i]=key;
                Value tvalue=value;
                tmpchild[i]=values->write(tvalue);
                for(i++;i<M;i++){
                    tmpkey[i]=currentNode.keys[i-1];
                    tmpchild[i]=currentNode.child[i-1];
                }
            }
            for(int i=0;i<HALFM;i++)split1.keys[i]=tmpkey[i];
            //memcpy(split1.keys,tmpkey,(HALFM)*sizeof(Key));
            memcpy(split1.child,tmpchild,(HALFM)*sizeof(int));
            for(int i=HALFM;i<M;i++)split2.keys[i-HALFM]=tmpkey[i];
            //memcpy(split2.keys,tmpkey+HALFM,(M-HALFM)*sizeof(Key));
            memcpy(split2.child,tmpchild+HALFM,(M-HALFM)*sizeof(int));
            split1.size=HALFM,split2.size=M-HALFM;
            int split2no;Key split2key=split2.keys[0];
            split2.pre=currentno;
            split2.child[M-1]=currentNode.child[M-1];
            split2no=nodes->write(split2);
            split1.child[M-1]=split2no;
            split1.pre=currentNode.pre;
            nodes->update(split1,currentno);
            int split2nextno=currentNode.child[M-1];
            if(split2nextno!=-1){
                node split2next;
                nodes->read(split2next,split2nextno);
                split2next.pre=split2no;
                nodes->update(split2next,split2nextno);
            }
            while(true){
                if(!path.empty()){
                    currentno=path.top().first;
                    nodes->read(currentNode,currentno);
                    if(currentNode.size<M-1){
                        for(int i=currentNode.size;i>=currentchildno+1;i--)currentNode.keys[i]=currentNode.keys[i-1];
                        //memmove(currentNode.keys+findPos+1,currentNode.keys+findPos,(currentNode.size-findPos)*sizeof(Key));
                        memmove(currentNode.child+currentchildno+2,currentNode.child+currentchildno+1,(currentNode.size-currentchildno)*sizeof(int));
                        currentNode.keys[currentchildno]=split2key;
                        currentNode.child[currentchildno+1]=split2no;
                        currentNode.size++;
                        nodes->update(currentNode,currentno);
                        break;
                    }
                    else{
                        split1=split2=node();
                        for(int i=0;i<currentchildno;i++)tmpkey[i]=currentNode.keys[i];
                        for(int i=0;i<=currentchildno;i++)tmpchild[i]=currentNode.child[i];
                        tmpkey[currentchildno]=split2key;
                        tmpchild[currentchildno+1]=split2no;
                        for(int i=currentchildno+1;i<=M;i++)
                            tmpkey[i]=currentNode.keys[i-1];
                        for(int i=currentchildno+2;i<=M+1;i++)
                            tmpchild[i]=currentNode.child[i-1];
                        for(int i=0;i<HALFMP1-1;i++)split1.keys[i]=tmpkey[i];
                        for(int i=0;i<=HALFMP1-1;i++)split1.child[i]=tmpchild[i];
                        split1.size=HALFMP1-1;
                        split2key=tmpkey[HALFMP1-1];
                        for(int i=HALFMP1;i<M;i++)split2.keys[i-HALFMP1]=tmpkey[i];
                        for(int i=HALFMP1;i<=M;i++)split2.child[i-HALFMP1]=tmpchild[i];
                        split2.size=M-HALFMP1;
                        split2no=nodes->write(split2);
                        nodes->update(split1,currentno);
                    }
                    currentchildno=path.top().second;
                    path.pop();
                }
                else{
                    node nroot;//new root
                    nroot.size=1;
                    nroot.child[0]=currentno;
                    nroot.keys[0]=split2key;
                    nroot.child[1]=split2no;
                    setRoot(nodes->write(nroot));
                    break;
                }
            }
            size++;
            delete[] tmpkey;
            delete[] tmpchild;
            return true;
        }

        bool modify(const Key &key, const Value &value) {
            if(!size)return false;
            int ptr=getRoot();
            node currentNode;
            nodes->read(currentNode,ptr);
            while(!currentNode.isleaf){
                int nextBranch=currentNode.size;//branch for next node to visit
                for(int i=0;i<M-1&&i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){nextBranch=i;break;}
                }
                //if(nextBranch==-1)return Value();
                nodes->read(currentNode,currentNode.child[nextBranch]);
            }
            int findPos=-1;
            for(int i=0;i<M-1&&i<currentNode.size;i++){
                if(currentNode.keys[i]==key){findPos=i;break;}
            }
            if(findPos==-1)return false;
            values->update(value,currentNode.child[findPos]);
        }

        Value at(const Key &key) {
            if(!size)return Value();
            int ptr=getRoot();
            node currentNode;
            nodes->read(currentNode,ptr);
            while(!currentNode.isleaf){
                int nextBranch=currentNode.size;//branch for next node to visit
                for(int i=0;i<M-1&&i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){nextBranch=i;break;}
                }
                //if(nextBranch==-1)return Value();
                nodes->read(currentNode,currentNode.child[nextBranch]);
            }
            int findPos=-1;
            for(int i=0;i<M-1&&i<currentNode.size;i++){
                if(currentNode.keys[i]==key){findPos=i;break;}
            }
            if(findPos==-1)return Value();
            Value ret;
            values->read(ret,currentNode.child[findPos]);
            return ret;
        }

        bool erase(const Key &key) {
            if(size==1){
                int rt=getRoot();
                node n;
                nodes->read(n,rt);
                if(n.keys[0]!=key)return false;
                clear();
                return true;
            }
            
            std::stack<std::pair<int,int>> path;//first:节点在nodes中的index second:节点在父节点中的child编号
            int ptr=getRoot();
            node currentNode;
            path.push(std::make_pair(ptr,-1));
            nodes->read(currentNode,ptr);
            while(!currentNode.isleaf){
                int nextBranch=currentNode.size;//branch for next node to visit
                for(int i=0;i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){nextBranch=i;break;}
                }
                path.push(std::make_pair(currentNode.child[nextBranch],nextBranch));
                nodes->read(currentNode,currentNode.child[nextBranch]);
            }
            int findPos=-1;
            for(int i=0;i<currentNode.size;i++){//TODO
                if(currentNode.keys[i]==key){findPos=i;break;}
            }
            if(findPos==-1)return false;
            size--;
            int currentno=path.top().first,currentchildno=path.top().second;
            
            if(currentNode.size>=HALFM||path.top().second==-1){
                for(int i=findPos;i<currentNode.size-1;i++){
                    currentNode.keys[i]=currentNode.keys[i+1];
                    currentNode.child[i]=currentNode.child[i+1];
                }
                currentNode.size--;
                nodes->update(currentNode,currentno);
                if(findPos==0&&path.top().second!=-1&&currentchildno>0){
                    path.pop();
                    int parentno=path.top().first;
                    node parentNode;
                    nodes->read(parentNode,parentno);
                    parentNode.keys[currentchildno-1]=currentNode.keys[0];
                    nodes->update(parentNode,parentno);
                }
                return true;
            }
            path.pop();
            //此时path必不为空，若为空则根节点为叶子节点，应已在上面处理完毕
            int parentno=path.top().first;
            node parentNode;
            nodes->read(parentNode,parentno);
            bool leftmost=(currentchildno==0),rightmost=(currentchildno==parentNode.size);
            int lneino=leftmost?-1:parentNode.child[currentchildno-1],rneino=rightmost?-1:parentNode.child[currentchildno+1];
            node lnei,rnei;//left/right neighbour
            if(!leftmost)nodes->read(lnei,lneino);
            if(!leftmost&&lnei.size>=HALFM){
                for(int i=findPos;i>0;i--){
                    currentNode.keys[i]=currentNode.keys[i-1];
                    currentNode.child[i]=currentNode.child[i-1];
                }
                currentNode.keys[0]=lnei.keys[lnei.size-1];
                currentNode.child[0]=lnei.child[lnei.size-1];
                lnei.size--;
                nodes->update(lnei,lneino);
                nodes->update(currentNode,currentno);
                parentNode.keys[currentchildno]=currentNode.keys[0];
                nodes->update(parentNode,parentno);
                return true;
            }
            if(!rightmost)nodes->read(rnei,rneino);
            if(!rightmost&&rnei.size>=HALFM){
                for(int i=findPos;i<currentNode.size-1;i++){
                    currentNode.keys[i]=currentNode.keys[i+1];
                    currentNode.child[i]=currentNode.child[i+1];
                }
                currentNode.keys[currentNode.size-1]=rnei.keys[0];
                currentNode.child[currentNode.size-1]=rnei.child[0];
                for(int i=0;i<rnei.size-1;i++){
                    rnei.keys[i]=rnei.keys[i+1];
                    rnei.child[i]=rnei.child[i+1];
                }
                rnei.size--;
                nodes->update(rnei,rneino);
                nodes->update(currentNode,currentno);
                parentNode.keys[currentchildno+1]=currentNode.keys[currentNode.size-1];
                nodes->update(parentNode,parentno);
                return true;
            }
            int childtodel;
            if(!leftmost){
                int pos=lnei.size;
                for(int i=0;i<currentNode.size;i++){
                    if(i==findPos)continue;
                    lnei.keys[pos]=currentNode.keys[i];
                    lnei.child[pos]=currentNode.child[i];
                    pos++;
                }
                lnei.size=pos;
                lnei.child[M-1]=currentNode.child[M-1];
                if(currentNode.child[M-1]!=-1){
                    node nextnode;
                    nodes->read(nextnode,currentNode.child[M-1]);
                    nextnode.pre=lneino;
                    nodes->update(nextnode,currentNode.child[M-1]);
                }
                nodes->update(lnei,lneino);
                nodes->Delete(currentno);
                childtodel=currentchildno;
            }
            else{
                for(int i=findPos;i<currentNode.size-1;i++){
                    currentNode.keys[i]=currentNode.keys[i+1];
                    currentNode.child[i]=currentNode.child[i+1];
                }
                int pos=currentNode.size-1;
                for(int i=0;i<rnei.size;i++){
                    currentNode.keys[pos]=rnei.keys[i];
                    currentNode.child[pos]=rnei.child[i];
                    pos++;
                }
                currentNode.size=pos;
                currentNode.child[M-1]=rnei.child[M-1];
                if(rnei.child[M-1]!=-1){
                    node rneinext;
                    nodes->read(rneinext,rnei.child[M-1]);
                    rneinext.pre=currentno;
                    nodes->update(rneinext,rnei.child[M-1]);
                }
                nodes->update(currentNode,currentno);
                nodes->Delete(rneino);
                childtodel=currentchildno+1;
            }
            while(true){
                currentno=parentno;
                currentNode=parentNode;
                currentchildno=path.top().second;
                path.pop();
                if(currentNode.size>=HALFM||path.empty()){
                    for(int i=childtodel-1;i<currentNode.size-1;i++)currentNode.keys[i]=currentNode.keys[i+1];
                    for(int i=childtodel;i<currentNode.size;i++)currentNode.child[i]=currentNode.child[i+1];
                    currentNode.size--;
                    if(path.empty()&&currentNode.size==0){
                        setRoot(currentNode.child[0]);
                        nodes->Delete(currentno);
                    }
                    nodes->update(currentNode,currentno);
                    break;
                }
                parentno=path.top().first;
                nodes->read(parentNode,parentno);
                bool leftmost=(currentchildno==0),rightmost=(currentchildno==parentNode.size);
                int lneino=leftmost?-1:parentNode.child[currentchildno-1],rneino=rightmost?-1:parentNode.child[currentchildno+1];
                if(!leftmost)nodes->read(lnei,lneino);
                if(!leftmost&&lnei.size>=HALFM){
                    for(int i=childtodel-1;i>0;i--)currentNode.keys[i]=currentNode.keys[i-1];
                    for(int i=childtodel;i>0;i--)currentNode.child[i]=currentNode.child[i-1];
                    currentNode.keys[0]=parentNode.keys[currentchildno-1];
                    parentNode.keys[currentchildno-1]=lnei.keys[lnei.size-1];
                    currentNode.child[0]=lnei.child[lnei.size];
                    lnei.size--;
                    nodes->update(lnei,lneino);
                    nodes->update(currentNode,currentno);
                    nodes->update(parentNode,parentno);
                    break;
                }
                if(!rightmost)nodes->read(rnei,rneino);
                if(!rightmost&&rnei.size>=HALFM){
                    for(int i=childtodel-1;i<currentNode.size-1;i++)currentNode.keys[i]=currentNode.keys[i+1];
                    for(int i=childtodel;i<currentNode.size;i++)currentNode.child[i]=currentNode.child[i+1];
                    currentNode.keys[currentNode.size-1]=parentNode.keys[currentchildno];
                    parentNode.keys[currentchildno]=rnei.keys[0];
                    currentNode.child[currentNode.size]=rnei.child[0];
                    for(int i=0;i<rnei.size-1;i++)rnei.keys[i]=rnei.keys[i+1];
                    for(int i=0;i<rnei.size;i++)rnei.child[i]=rnei.child[i+1];
                    rnei.size--;
                    nodes->update(rnei,rneino);
                    nodes->update(currentNode,currentno);
                    nodes->update(parentNode,parentno);
                    break;
                }
                if(!leftmost){
                    int pos=lnei.size;
                    lnei.keys[pos++]=parentNode.keys[currentchildno-1];
                    for(int i=0;i<currentNode.size;i++){
                        if(i==childtodel-1)continue;
                        lnei.keys[pos]=currentNode.keys[i];
                        pos++;
                    }
                    pos=lnei.size+1;
                    for(int i=0;i<=currentNode.size;i++){
                        if(i==childtodel)continue;
                        lnei.child[pos]=currentNode.child[i];
                        pos++;
                    }
                    lnei.size=pos-1;
                    nodes->update(lnei,lneino);
                    nodes->Delete(currentno);
                    childtodel=currentchildno;
                }
                else{
                    for(int i=childtodel-1;i<currentNode.size-1;i++)currentNode.keys[i]=currentNode.keys[i+1];
                    for(int i=childtodel;i<currentNode.size;i++)currentNode.child[i]=currentNode.child[i+1];
                    int pos=currentNode.size;
                    currentNode.keys[pos++]=parentNode.keys[currentchildno];
                    for(int i=0;i<rnei.size;i++)currentNode.keys[pos++]=rnei.keys[i];
                    pos=currentNode.size+1;
                    for(int i=0;i<=rnei.size;i++)currentNode.child[pos++]=rnei.child[i];
                    currentNode.size=pos-1;
                    nodes->update(currentNode,currentno);
                    nodes->Delete(rneino);
                    childtodel=currentchildno+1;
                }
            }
            return true;
        }

        int getSize(){
            return size;
        }
        
        
        class iterator {
        friend class BTree;
        private:
            // Your private members go here
            BTree *tree=nullptr;
            mutable int nod,pos;
            mutable int valuePos;
            Key key;
            mutable int nodsiz;
            
            bool update()const{
                if(!tree)return false;
                if(nod==-1)return true;
                node n;
                tree->nodes->read(n,nod);
                nodsiz=n.size;
                if(n.keys[pos]!=key){
                    auto updated=tree->find(key);
                    nod=updated.nod,pos=updated.pos,nodsiz=updated.nodsiz,valuePos=updated.valuePos;
                    return(nod!=-1);
                }
                return true;
            }
            bool updateKey(){
                if(!tree)return false;
                node n;
                tree->nodes->read(n,nod);
                nodsiz=n.size;
                key=n.keys[pos];
                valuePos=n.child[pos];
                if(n.keys[pos]!=key){
                    auto updated=tree->find(key);
                    nod=updated.nod,pos=updated.pos,nodsiz=updated.nodsiz,valuePos=updated.valuePos;
                    return(nod!=-1);
                }
                return true;
            }
        public:
            iterator(BTree *bt=nullptr,int nd=-1,int ps=-1,int vps=-1,const Key &k=Key(),int ndsz=0):tree(bt),nod(nd),pos(ps),valuePos(vps),key(k),nodsiz(ndsz){}
            
            iterator(const iterator& other):tree(other.tree),nod(other.nod),pos(other.pos),valuePos(other.valuePos),key(other.key),nodsiz(other.nodsiz) {
                
            }

            // modify by iterator
            bool modify(const Value& value) {
                if(!update())return false;
                values->update(value,valuePos);
                return true;
            }

            Key getKey() const {
                if(!update())throw invalid_iterator();
                return key;
            }

            Value getValue() const {
                if(!update())throw invalid_iterator();
                Value v;
                tree->values->read(v,valuePos);
                return v;
            }

            iterator operator++(int) {
                if(!update())throw invalid_iterator();
                iterator ret=(*this);
                if(pos<nodsiz-1)pos++;
                else{
                    node n;
                    tree->nodes->read(n,nod);
                    nod=n.child[M-1];
                    pos=0;
                }
                updateKey();
                return ret;
            }

            iterator& operator++() {
                if(!update())throw invalid_iterator();
                if(pos<nodsiz-1)pos++;
                else{
                    node n;
                    nodes->read(n,nod);
                    nod=n.child[M-1];
                    pos=0;
                    /*if(nod!=-1){
                        nodes->read(n,nod);
                        nodsiz=n.size;
                    }*/
                }
                updateKey();
                return *this;
            }
            iterator operator--(int) {
                if(!update())throw invalid_iterator();
                iterator ret=*this;
                if(pos>0)pos--;
                else{
                    node n;
                    nodes->read(n,nod);
                    nod=n.pre;
                    if(nod!=-1){
                        nodes->read(n,nod);
                        nodsiz=n.size;
                    }
                }
                updateKey();
                return ret;
            }

            iterator& operator--() {
                if(!update())throw invalid_iterator();
                if(pos>0)pos--;
                else{
                    node n;
                    nodes->read(n,nod);
                    nod=n.pre;
                    if(nod!=-1){
                        nodes->read(n,nod);
                        nodsiz=n.size;
                    }
                }
                updateKey();
                return *this;
            }

            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                update();
                rhs.update();
                if(tree!=rhs.tree)return false;
                if(nod==-1&&rhs.nod==-1)return true;
                return(nod==rhs.nod&&pos==rhs.pos);
            }

            bool operator!=(const iterator& rhs) const {
                return !(*this==rhs);
            }
        };
        
        iterator begin() {
            if(!size)return end();
            int no=getRoot();
            node currentNode;
            nodes->read(currentNode,no);
            while(!currentNode.isleaf){
                no=currentNode.child[0];
                nodes->read(currentNode,currentNode.child[0]);
            }
            return iterator(this,no,0,currentNode.child[0],currentNode.keys[0],currentNode.size);
        }
        
        // return an iterator to the end(the next element after the last)
        iterator end() {
            return iterator(this,-1,-1,-1,Key(),0);
        }

        iterator find(const Key &key) {
            if(!size)return end();
            int ptr=getRoot();
            node currentNode;
            nodes->read(currentNode,ptr);
            while(!currentNode.isleaf){
                int nextBranch=currentNode.size;//branch for next node to visit
                for(int i=0;i<currentNode.size;i++){
                    if(currentNode.keys[i]>key){nextBranch=i;break;}
                }
                ptr=currentNode.child[nextBranch];
                nodes->read(currentNode,currentNode.child[nextBranch]);
            }
            int findPos=-1;
            for(int i=0;i<currentNode.size;i++){//TODO
                if(currentNode.keys[i]==key){findPos=i;break;}
            }
            if(findPos==-1)return end();
            return iterator(this,ptr,findPos,currentNode.child[findPos],key,currentNode.size);
        }
        
        // return an iterator whose key is the smallest key greater or equal than 'key'
        iterator lower_bound(const Key &key) {
            if(!size)return end();
            return lower_bound(getRoot(),key);
        }
    private:
        iterator lower_bound(int nod,const Key &k){
            node n;
            nodes->read(n,nod);
            if(n.isleaf){
                int findPos=-1;
                for(int i=0;i<n.size;i++)if(n.keys[i]>=k){findPos=i;break;}
                if(findPos==-1)return end();
                return iterator(this,nod,findPos,n.child[findPos],n.keys[findPos],n.size);
            }
            int findPos=n.size;
            for(int i=0;i<n.size;i++)if(n.keys[i]>=k){findPos=i;break;}
            iterator ret1=lower_bound(n.child[findPos],k),ret2;
            if(findPos!=n.size)ret2=lower_bound(n.child[findPos+1],k);
            else ret2=end();
            if(ret1==end()){
                return ret2;
            }
            if(ret2==end()){
                return ret1;
            }
            return (ret1.key<ret2.key)?ret1:ret2;
        }
    };
    
}  // namespace sjtu



#endif
