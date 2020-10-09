#include <functional>
#include <cstddef>
#include "exception.hpp"

#define m_v 227
#define offset int
#define nulloffset -1
enum TYPE { internal_type, leaf_type };
int max(int x, int y) { return x > y ? x : y; }

namespace sjtu {

    template <class Key, class Value>
    class BTree {
    private:
        // Your private members go here
        struct node
        {
            offset pre, nxt;//左右节点指针（外存形式）
            offset addr;//节点外存地址
            TYPE t;//节点类型
            char* data;//数据串
            int scale;//单个数据数据规模
            int len;//键值个数
            int pointlen;//儿子个数
            node* pointer[m_v + 2];//下层指针
            node(TYPE ti = internal_type)
            {
                t = ti; len = 0; pointlen = 0;
                scale = max(sizeof(offset), sizeof(Value));
                pre = nxt = nulloffset;
                addr = 0;
                data = new char[sizeof(Key) * (m_v + 1) + scale * (m_v + 1)];
                memset(data, 0, sizeof(Key) * (m_v + 1) + scale * (m_v + 1));
                memset(pointer, 0, (m_v + 2) * sizeof(node*));
            }
            node(const node& x)
            {
                t = x.t; len = x.len; pointlen = x.pointlen;
                scale = x.scale;
                pre = x.pre; nxt = x.nxt;
                addr = x.addr;
                data = new char[sizeof(Key) * (m_v + 1) + scale * (m_v + 1)];
                memcpy(data, x.data, sizeof(Key) * (m_v + 1) + scale * (m_v + 1));
                memcpy(pointer, x.pointer, (m_v + 2) * sizeof(node*));
            }
            ~node() { delete data; }
            node& operator=(const node& x)
            {
                if (&x == this) return *this;
                t = x.t; len = x.len; pointlen = x.pointlen;
                scale = x.scale;
                pre = x.pre; nxt = x.nxt;
                addr = x.addr;
                data = new char[sizeof(Key) * (m_v + 1) + scale * (m_v + 1)];
                memcpy(data, x.data, sizeof(Key) * (m_v + 1) + scale * (m_v + 1));
                memcpy(pointer, x.pointer, (m_v + 2) * sizeof(node*));
            }

            Key _key(int n)const
            {
                int p = (n - 1) * sizeof(Key) + (n - 1) * scale;
                char re[sizeof(Key)];
                memcpy(re, data + p, sizeof(Key));
                return *(reinterpret_cast<Key*>(re));
            }
            void ckey(int n, Key x)
            {
                int p = (n - 1) * sizeof(Key) + (n - 1) * scale;
                memcpy(data + p, reinterpret_cast<char*>(&x), sizeof(Key));
            }
            Value _value(int n)const
            {
                int p = n * sizeof(Key) + (n - 1) * scale;
                char re[sizeof(Value)];
                memcpy(re, data + p, sizeof(Value));
                return *(reinterpret_cast<Value*>(re));
            }
            void cvalue(int n, Value x)
            {
                int p = n * sizeof(Key) + (n - 1) * scale;
                memcpy(data + p, reinterpret_cast<char*>(&x), sizeof(Value));
            }
            offset _Offset(int n)const
            {
                int p = n * sizeof(Key) + (n - 1) * scale;
                char re[sizeof(offset)];
                memcpy(re, data + p, sizeof(offset));
                return *(reinterpret_cast<offset*>(re));
            }
            void cOffset(int n, offset x)
            {
                int p = n * sizeof(Key) + (n - 1) * scale;
                memcpy(data + p, reinterpret_cast<char*>(&x), sizeof(offset));
            }

            void r_move(int n)//信息右移
            {
                int p = (n - 1) * sizeof(Key) + (n - 1) * scale;
                memmove(data + p + sizeof(Key) + scale, data + p, (len - n + 1) * sizeof(Key) + (len - n + 1) * scale);
                memmove(pointer + n + 1, pointer + n, (len - n + 1) * sizeof(node*));
            }
            void l_move(int n)
            {
                int p = (n - 1) * sizeof(Key) + (n - 1) * scale;
                memmove(data + p, data + p + sizeof(Key) + scale, (len - n) * sizeof(Key) + (len - n) * scale);
                memmove(pointer + n, pointer + n + 1, (len - n) * sizeof(node*));
            }
        };
        FILE* file;
        char filename[55];
        node* root;
        node* tail;
        int sz;
        offset site;

        void write_block(node& x)
        {
            fseek(file, x.addr, SEEK_SET);
            fwrite(&x.pre, sizeof(offset), 1, file);
            fwrite(&x.nxt, sizeof(offset), 1, file);
            fwrite(&x.len, sizeof(int), 1, file);
            fwrite(&x.pointlen, sizeof(int), 1, file);
            fwrite(&x.t, sizeof(TYPE), 1, file);
            fwrite(x.data, 1, sizeof(Key) * (m_v + 1) + x.scale * (m_v + 1), file);
        }
        void get_block(offset off, node& x)
        {
            x.addr = off;
            fseek(file, off, SEEK_SET);
            fread(&x.pre, sizeof(offset), 1, file);
            fread(&x.nxt, sizeof(offset), 1, file);
            fread(&x.len, sizeof(int), 1, file);
            fread(&x.pointlen, sizeof(int), 1, file);
            fread(&x.t, sizeof(TYPE), 1, file);
            fread(x.data, 1, sizeof(Key) * (m_v + 1) + x.scale * (m_v + 1), file);
        }

        void clear(node* p)
        {
            for (int i = p->pointlen; i > 0; i--)
            {
                if (p->pointer[i] != NULL)
                    clear(p->pointer[i]);
            }
            delete p;
        }

        bool findb(const Key& key)
        {
            node* p = root;
            int i;
            while (1)
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i)) i--;
                if (p->t == leaf_type)
                    break;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                p = p->pointer[i];
            }
            while (i >= 1 && key < p->_key(i)) i--;
            if (i == 0 || (i >= 1 && p->_key(i) != key)) return 0;
            return 1;
        }

        offset nextaddr()
        {
            return sizeof(int) + sizeof(offset) + (sizeof(offset) * 2 + sizeof(int) * 2 + sizeof(TYPE) + sizeof(Key) * (m_v + 1) + max(sizeof(Value), sizeof(offset)) * (m_v + 1)) * sz;
            //sz           site              pre,nxt            len,pointlen        t                               data
        }

        void split(node* fp, node* p, int i)
        {
            node* np = new node;
            np->addr = nextaddr();//得到新的地址序号
            sz++;
            np->t = p->t;//分裂在右侧
            np->nxt = p->nxt;
            np->pre = p->addr;
            p->nxt = np->addr;
            int fl = p->len;
            p->len = fl / 2;
            np->len = fl - p->len;
            fl = p->pointlen;
            p->pointlen = fl / 2;
            np->pointlen = fl - p->pointlen;
            memcpy(np->data, p->data + p->pointlen * (sizeof(Key) + p->scale), p->pointlen * (sizeof(Key) + p->scale));
            memcpy(np->pointer + 1, p->pointer + p->pointlen + 1, p->pointlen * sizeof(node*));
            fp->r_move(i + 1);
            fp->ckey(i + 1, np->_key(1));
            fp->cOffset(i + 1, np->addr);
            fp->pointer[i + 1] = np;
            fp->len++;
            fp->pointlen++;
            write_block(*fp);
            write_block(*p);
            write_block(*np);
            fseek(file, 0, SEEK_SET);
            fwrite(&sz, sizeof(int), 1, file);
            fwrite(&site, sizeof(offset), 1, file);
        }


        bool insert(node* p, const Key& key, const Value& value)
        {
            int i;
            if (p->t == leaf_type)
            {
                i = p->len;;
                while (i >= 1 && key < p->_key(i)) i--;
                if (p->len == 0)
                {
                    p->ckey(1, key);
                    p->cvalue(1, value);
                    p->len++;
                    p->pointlen++;
                }
                else
                {
                    p->r_move(i + 1);
                    p->ckey(i + 1, key);
                    p->cvalue(i + 1, value);
                    p->len++;
                    p->pointlen++;
                }
                write_block(*p);
                if (p->len == m_v + 1) return 1;
                else return 0;
            }
            else
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i))i--;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                if (insert(p->pointer[i], key, value))
                {
                    split(p, p->pointer[i], i);//在上层做分裂.避免记录fa
                    if (p->len == m_v + 1) return 1;
                    return 0;
                }
                else return 0;
            }
        }

        int del(node* p, const Key& key)//此处键值到m_v/2就要借，因为一号指针没有键值
        {
            int i;
            if (p->t == leaf_type)
            {
                i = p->len;
                while (i >= 1 && key < p->_key(i)) i--;
                p->l_move(i);
                p->len--;
                p->pointlen--;
                write_block(*p);
                if (i == 1)
                {
                    if (p->len >= m_v / 2 + 1) return 1;//键值修改
                    if (p->len == m_v / 2) return 2;//同层合并+键值修改
                }
                else
                {
                    if (p->len >= m_v / 2 + 1) return 3;//无需做操作
                    if (p->len == m_v / 2) return 4;//同层合并
                }
            }
            else
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i))i--;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                int re = del(p->pointer[i], key);
                if (re == 3) return 3;
                else if (re == 1)
                {
                    p->ckey(i, p->pointer[i]->_key(1));
                    write_block(*p);
                    if (i == 1) return 1;
                    else return 3;
                }
                else
                {
                    if (re == 2) p->ckey(i, p->pointer[i]->_key(1));
                    if (i == 1)//左侧优先，特判右侧
                    {
                        if (p->pointer[2] == NULL)
                        {
                            p->pointer[2] = new node;
                            get_block(p->_Offset(2), *(p->pointer[2]));
                        }
                        if (p->pointer[2]->len == m_v / 2 + 1)
                        {
                            int pos = (m_v / 2) * sizeof(Key) + (m_v / 2) * p->scale;
                            memmove(p->pointer[1]->data + pos, p->pointer[2]->data, (m_v / 2 + 1) * sizeof(Key) + (m_v / 2 + 1) * p->scale);
                            memmove(p->pointer[1]->pointer + 1 + m_v / 2, p->pointer[2]->pointer + 1, (m_v / 2 + 1) * sizeof(node*));
                            p->pointer[1]->len += m_v / 2 + 1;
                            p->pointer[1]->pointlen += m_v / 2 + 1;
                            delete p->pointer[2]; p->pointer[2] = NULL;
                            p->l_move(2);
                            p->len--;
                            p->pointlen--;
                            write_block(*(p->pointer[1]));
                        }
                        else
                        {
                            int pos = (m_v / 2) * sizeof(Key) + (m_v / 2) * p->scale;
                            memmove(p->pointer[1]->data + pos, p->pointer[2]->data, sizeof(Key) + p->scale);
                            memmove(p->pointer[1]->pointer + m_v / 2 + 1, p->pointer[2]->pointer + 1, sizeof(node*));
                            p->pointer[2]->l_move(1);
                            p->ckey(2, p->pointer[2]->_key(1));
                            p->pointer[1]->len++;
                            p->pointer[2]->len--;
                            p->pointer[1]->pointlen++;
                            p->pointer[2]->pointlen--;
                            write_block(*(p->pointer[1]));
                            write_block(*(p->pointer[2]));
                        }
                    }
                    else
                    {
                        if (p->pointer[i - 1] == NULL)
                        {
                            p->pointer[i - 1] = new node;
                            get_block(p->_Offset(i - 1), *(p->pointer[i - 1]));
                        }
                        if (p->pointer[i - 1]->len == m_v / 2 + 1)
                        {
                            int pos = (m_v / 2 + 1) * sizeof(Key) + (m_v / 2 + 1) * p->scale;
                            memmove(p->pointer[i - 1]->data + pos, p->pointer[i]->data, (m_v / 2) * sizeof(Key) + (m_v / 2) * p->scale);
                            memmove(p->pointer[i - 1]->pointer + 2 + m_v / 2, p->pointer[i]->pointer + 1, (m_v / 2) * sizeof(node*));
                            p->pointer[i - 1]->len += m_v / 2;
                            p->pointer[i - 1]->pointlen += m_v / 2;
                            delete p->pointer[i]; p->pointer[i] = NULL;
                            p->l_move(i);
                            p->len--;
                            p->pointlen--;
                            write_block(*(p->pointer[i - 1]));
                        }
                        else
                        {
                            int li = p->pointer[i - 1]->len;
                            p->pointer[i]->r_move(1);
                            memmove(p->pointer[i]->data, p->pointer[i - 1]->data + (li - 1) * (sizeof(Key) + p->scale), sizeof(Key) + p->scale);
                            memmove(p->pointer[i]->pointer + 1, p->pointer[i - 1]->pointer + li, sizeof(node*));
                            p->ckey(i, p->pointer[i]->_key(1));
                            p->pointer[i]->len++;
                            p->pointer[i - 1]->len--;
                            p->pointer[i]->pointlen++;
                            p->pointer[i - 1]->pointlen--;
                            write_block(*(p->pointer[i]));
                            write_block(*(p->pointer[i - 1]));
                        }
                    }
                    write_block(*p);
                    if (re == 2 && i == 1 && p->len == m_v / 2)
                        return 2;
                    else if (re == 2 && i == 1)
                        return 1;
                    else if (p->len == m_v / 2)
                        return 4;
                    else
                        return 3;
                }
            }
        }

    public:
        BTree()
        {
            FILE* on1 = fopen("BPlustree", "rb");
            if (on1)
            {
                fclose(on1);
                strcpy(filename, "BPlustree");
                file = fopen(filename, "rb+");
                root = new node(leaf_type);
                fseek(file, 0, SEEK_SET);
                fread(&sz, sizeof(int), 1, file);
                fread(&site, sizeof(offset), 1, file);
                get_block(site, *root);//读取根节点
            }
            else
            {
                strcpy(filename, "BPlustree");
                root = new node(leaf_type);
                root->addr = sizeof(int) + sizeof(offset);
                root->pointlen = root->len = 0;
                sz = 1; site = root->addr;
                file = fopen("BPlustree", "wb+");
                fwrite(&sz, sizeof(int), 1, file);
                fwrite(&site, sizeof(offset), 1, file);
                write_block(*root);
            }
        }

        BTree(const char* fname) {
            FILE* on1 = fopen(fname, "rb");
            if (on1)
            {
                fclose(on1);
                strcpy(filename, fname);
                file = fopen(filename, "rb+");
                root = new node(leaf_type);
                fseek(file, 0, SEEK_SET);
                fread(&sz, sizeof(int), 1, file);
                fread(&site, sizeof(offset), 1, file);
                get_block(site, *root);//读取根节点
            }
            else
            {
                strcpy(filename, fname);
                root = new node(leaf_type);
                root->addr = sizeof(int) + sizeof(offset);
                root->pointlen = root->len = 0;
                sz = 1; site = root->addr;
                file = fopen(fname, "wb+");
                fwrite(&sz, sizeof(int), 1, file);
                fwrite(&site, sizeof(offset), 1, file);
                write_block(*root);
            }
        }

        ~BTree() {
            clear(root);
            fclose(file);
        }

        // Clear the BTree
        void clear() {
            clear(root);
            fclose(file);
            root = new node(leaf_type);//初始化根节点信息
            root->addr = sizeof(int) + sizeof(offset);
            root->pointlen = root->len = 0;
            sz = 1; site = root->addr;
            file = fopen(filename, "wb+");
            fwrite(&sz, sizeof(int), 1, file);
            fwrite(&site, sizeof(offset), 1, file);
            write_block(*root);
        }

        bool insert(const Key& key, const Value& value)
        {
            if (findb(key))
                return 0;
            else
            {
                if (insert(root, key, value))//根节点是否要分裂
                {
                    node* nr = new node;
                    node* np = new node;
                    np->addr = nextaddr();
                    sz++;
                    np->t = root->t;
                    np->nxt = root->nxt;
                    np->pre = root->addr;
                    root->nxt = np->addr;
                    int fl = root->len;
                    root->len = fl / 2;
                    np->len = fl - root->len;
                    fl = root->pointlen;
                    root->pointlen = fl / 2;
                    np->pointlen = fl - root->pointlen;
                    memcpy(np->data, root->data + root->pointlen * (sizeof(Key) + root->scale), root->pointlen * (sizeof(Key) + np->scale));
                    memcpy(np->pointer + 1, root->pointer + root->pointlen + 1, root->pointlen * sizeof(node*));
                    nr->addr = nextaddr();
                    sz++;
                    nr->len = 2;
                    nr->pointlen = 2;
                    nr->cOffset(1, root->addr);
                    nr->cOffset(2, np->addr);
                    nr->pointer[1] = root;
                    nr->pointer[2] = np;
                    nr->ckey(1, root->_key(1));
                    nr->ckey(2, np->_key(1));
                    write_block(*nr);
                    write_block(*root);
                    write_block(*np);
                    root = nr;
                    site = nr->addr;
                    fseek(file, 0, SEEK_SET);
                    fwrite(&sz, sizeof(int), 1, file);
                    fwrite(&site, sizeof(offset), 1, file);
                }
                return 1;
            }
        }

        bool modify(const Key& key, const Value& value)
        {
            node* p = root;
            int i;
            while (1)
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i)) i--;
                if (p->t == leaf_type) break;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(), *(p->pointer[i]));
                }
                p = p->pointer[i];
            }
            if (p->_key(i) != key) return 0;
            else
            {
                p->cvalue(i, value);
                write_block(p);
                return 1;
            }
        }

        Value at(const Key& key)
        {
            node* p = root;
            int i;
            while (1)
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i)) i--;
                if (p->t == leaf_type) break;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                p = p->pointer[i];
            }
            if (p->_key(i) != key) return Value();
            else return p->_value(i);
        }

        bool erase(const Key& key)
        {
            if (!findb(key)) return 0;
            else
            {
                del(root, key);
                if (root->len == 1)//避免B+树只剩一个节点时无法借的情况，导致节点键值<m_v/2的情况
                {
                    node* p = root->pointer[1];
                    delete root;
                    root = p;
                    site = root->addr;
                    fseek(file, 0, SEEK_SET);
                    fwrite(&sz, sizeof(int), 1, file);
                    fwrite(&site, sizeof(offset), 1, file);
                }
            }
        }

        class iterator {
            friend class BTree;
        private:
            BTree* bp;
            node* p;
            int i;
            // Your private members go here
        public:

            iterator() {
                bp = NULL; p = NULL; i = 1;
            }
            iterator(const iterator& other) {
                bp = other.bp;
                p = new node(*(other.p));
                i = other.i;
            }
            iterator& operator=(const iterator& other) {
                if (&other == this) return *this;
                delete p;
                bp = other.bp;
                p = new node(*(other.p));
                i = other.i;
            }
            ~iterator() { delete p; }

            // modify by iterator
            bool modify(const Value& value) {
                if (i <= p->pointlen)
                {
                    p->cvalue(i, value);
                    fseek(bp->file, p->addr, SEEK_SET);
                    fwrite(p->data, 1, sizeof(Key) * (m_v + 1) + p->scale * (m_v + 1), bp->file);
                    return 1;
                }
                return 0;
            }

            Key getKey() const {
                return p->_key(i);
            }

            Value getValue() const {
                return p->_value(i);
            }

            iterator operator++(int) {
                iterator re = *this;
                if (i < p->pointlen) 
                { 
                    i++; 
                    return re; 
                }
                else if (p->nxt == nulloffset) 
                { 
                    i++; 
                    return re; 
                }
                else 
                {
                    bp->get_block(p->nxt, *p);
                    i = 1;
                    return re;
                }
            }

            iterator& operator++() {
                if (i < p->pointlen)
                {
                    i++;
                    return *this;
                }
                else if (p->nxt == nulloffset)
                {
                    i++;
                    return *this;
                }
                else
                {
                    bp->get_block(p->nxt, *p);
                    i = 1;
                    return *this;
                }
            }
            iterator operator--(int) {
                iterator re = *this;
                if (i == 1)
                {
                    bp->get_block(p->pre, *p);
                    i = p->pointlen;
                    return re;
                }
                else 
                {
                    i--;
                    return re;
                }
            }

            iterator& operator--() {
                if (i == 1)
                {
                    bp->get_block(p->pre, *p);
                    i = p->pointlen;
                    return *this;
                }
                else
                {
                    i--;
                    return *this;
                }
            }

            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return (p->addr == rhs.p->addr && bp == rhs.bp && i == rhs.i);
            }

            bool operator!=(const iterator& rhs) const {
                return !(p->addr == rhs.p->addr && bp == rhs.bp && i == rhs.i);
            }
        };

        iterator begin() 
        {
            iterator re;
            re.bp = this;
            node* p;
            p = root;
            while (p->t != leaf_type)
            {
                if (p->pointer[1] == NULL)
                {
                    p->pointer[1] = new node;
                    get_block(p->_Offset(1), *(p->pointer[1]));
                }
                p = p->pointer[1];
            }
            re.p = new node(*p); re.i = 1;
            return re;
        }

        // return an iterator to the end(the next element after the last)
        iterator end() 
        {
            iterator re;
            re.bp = this;
            node* p;
            p = root;
            while (p->t != leaf_type)
            {
                if (p->pointer[p->pointlen] == NULL)
                {
                    p->pointer[p->pointlen] = new node;
                    get_block(p->_Offset(p->pointlen), *(p->pointer[p->pointlen]));
                }
                p = p->pointer[p->pointlen];
            }
            re.p =new node(*p); re.i = p->pointlen+1;
            return re;
        }

        iterator find(const Key& key) 
        {
            iterator re;
            re.bp = this;
            node* p = root;
            int i;
            while (1) 
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i))i--;
                if (p->t==leaf_type)
                    break;
                if (p->pointer[i] == NULL) 
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                p = p->pointer[i];
            }
            re.p = new node(*p);
            re.i = i;
            return re;
        }

        // return an iterator whose key is the smallest key greater or equal than 'key'
        iterator lower_bound(const Key& key) 
        {
            iterator re;
            re.bp = this;
            node* p = root;
            int i;
            while (1)
            {
                i = p->len;
                while (i >= 2 && key < p->_key(i))i--;
                if (p->t == leaf_type)
                    break;
                if (p->pointer[i] == NULL)
                {
                    p->pointer[i] = new node;
                    get_block(p->_Offset(i), *(p->pointer[i]));
                }
                p = p->pointer[i];
            }
            re.p = new node(*p);
            int id = 1;
            while (id <= p->pointlen && p->_key(id) < key)++id;
            if (id == p->pointlen + 1)
            {
                re.i = p->pointlen;
                ++re;
            }
            else
                re.i = id;
            return re;
        }

    };
}  // namespace sjtu