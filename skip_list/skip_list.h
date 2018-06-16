/*
 * File        : skip_list.h
 * Created Date: 2018-04-20 20:13:43
 * Author      : philma
 * Desc        : 基于一段连续内存的跳跃表，支持重复元素，支持迭代器遍历，支持自定义排序
 */

#include <string>
#include <cstring>

template<typename T, typename Compare = std::less<T>>
class SkipList
{
    static const int MAX_LEVEL_NUM = 32;        // 跳跃表的最大层数
    static const int SKIPLIST_P = 4;            // 跳跃表随机层数，每增加一层的概率，多少分之一

public:
    class Iterator;
    
    /*
     * 初始化跳跃表
     */
    bool init(void* mem, size_t mem_size, uint32_t max_sl_len, bool is_raw = true);

    /*
     * 查找元素在跳跃表中的位置索引（如果有多个相同元素，取排在最前面元素的位置），索引值从1开始
     * 返回0表示没找到
     */
    uint32_t get_index(const T& element) const;

    /*
     * 根据元素查找跳跃表，返回元素所在位置的迭代器（如果有多个相同元素，取排在最前面元素的位置）
     * 找不到则返回迭代器同end函数
     */
    Iterator find(const T& element) const;

    /*
     * 根据位置索引查找跳跃表，返回对应位置的迭代器，位置索引从1开始
     * 不在[1, length]范围内的索引，返回迭代器同end函数
     */
    Iterator find(uint32_t index) const;

    /*
     * 插入一个元素，支持相同的元素插入，后插入的相同元素排在先插入的前面
     * 返回false表示插入失败，仅在空间不足的情况下发生
     */
    bool insert(const T& element);

    /*
     * 删除元素，如果有多个相同元素，则均删除
     * 返回删除的元素个数
     */
    uint32_t erase(const T& element);

    /*
     * 根据跳跃表的最大长度，获取需要的最大内存大小
     */
    size_t max_mem_size(uint32_t max_sl_len) const
    {
        size_t size = mem_header_size();
        size += (max_sl_len + 1) * mem_node_size();

        return size;
    }

    /*
     * 获取跳跃表内部的错误信息
     */
    const std::string& err_msg() const { return err_msg_; }

    Iterator begin() const
    {
        MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
        return Iterator(this, node->sl_node_info.level[0].forward);
    }

    Iterator end() const
    {
        if(mem_header_->sl_info.tail)
        {
            MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.tail));
            return Iterator(this, node->sl_node_info.level[0].forward);
        }
        else
            return Iterator(this, mem_header_->sl_info.tail);
    }

public:

    class Iterator
    {
    public:
        Iterator(const SkipList* skip_list_, size_t node_ref_)
            :skip_list(skip_list_), node_ref(node_ref_)
        {}

        T& operator*() const
        {
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            return node->sl_node_info.element;
        }

        T* operator->() const
        {
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            return &(node->sl_node_info.element);
        }

        bool operator==(const Iterator& rhs) const
        {
            return (skip_list == rhs.skip_list
                && node_ref == rhs.node_ref);
        }

        bool operator!=(const Iterator& rhs) const
        {
            return (skip_list != rhs.skip_list
                || node_ref != rhs.node_ref);
        }

        Iterator& operator++()
        {
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            node_ref = node->sl_node_info.level[0].forward;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            node_ref = node->sl_node_info.level[0].forward;
            return tmp;
        }

        Iterator& operator--()
        {
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            node_ref = node->sl_node_info.backword;
            return *this;             
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            MemNode* node = reinterpret_cast<MemNode*>(skip_list->deref(node_ref));
            node_ref = node->sl_node_info.backword;
            return tmp;
        }

    private:
        const SkipList* skip_list;
        size_t node_ref;    
    };

private:

    // 删除一个元素，如果有多个，删除排在最前的那个；返回false表示没找到要删除的元素
    bool del_first_of(const T& element);

private:

    static const uint32_t MAGIC_NUM = 0x12345678;

    struct SLInfo
    {
        size_t head;                    // 跳跃表头节点偏移
        size_t tail;                    // 跳跃表尾节点偏移
        int level_num;                  // 跳跃表当前的层数
        uint32_t length;                // 跳跃表当前的长度，即表中元素个数
    };

    struct SLLevel
    {
        size_t forward;                 // 指向跳到的下一个跳跃表节点
        uint32_t span;                  // 跳跃的跨度
    };

    struct SLNodeInfo
    {
        T element;                      // 跳跃表中存放的元素
        size_t backword;                // 指向前一个跳跃表节点，用于逆向遍历
        SLLevel level[MAX_LEVEL_NUM];   // 跳跃表节点中的层
    };

    struct MemHeader
    {
        uint32_t magic_num;
        size_t mem_size;                // 总内存大小
        size_t alloc_size;              // 已申请大小，包括空闲节点
        size_t header_size;             // 内存头部大小
        size_t node_size;               // 节点大小
        size_t free_list;               // 空闲节点列表，为0表示列表为空
        SLInfo sl_info;                 // 跳跃表的信息
    };

    struct MemNode
    {
        SLNodeInfo sl_node_info;        // 跳跃表节点信息
        size_t next;                    // 空闲列表中，下一节点的偏移
    };

    size_t mem_header_size() const
    {
        return (sizeof(MemHeader) + 7) & (~7);
    }

    size_t mem_node_size() const
    {
        return (sizeof(MemNode) + 7) & (~7);
    }

    size_t ref(void* p) const
    {
        return reinterpret_cast<char*>(p) - reinterpret_cast<char*>(mem_header_);
    }

    void* deref(size_t ref) const
    {
        return reinterpret_cast<char*>(mem_header_) + ref;
    }

    int random_level() const
    {
        int level = 1;
        while((random() & 0xFFFF) < (1.0 / SKIPLIST_P * 0xFFFF))
            level += 1;
        
        return (level < MAX_LEVEL_NUM ? level : MAX_LEVEL_NUM);
    }

    // 申请一个内存节点，返回节点的偏移；返回0表示内存不够了，申请失败
    size_t alloc_node();

    // 释放一个内存节点到空闲链表
    void free_node(size_t node_ref);

private:
    MemHeader* mem_header_ = nullptr;
    std::string err_msg_;
};

template<typename T, typename Compare>
bool SkipList<T, Compare>::init(void* mem, size_t mem_size, uint32_t max_sl_len, bool is_raw)
{
    if(!mem)
    {
        err_msg_ = "mem is nullptr";
        return false;
    }

    if(mem_size < max_mem_size(max_sl_len))
    {
        err_msg_ = "mem_size not enough";
        return false;
    }

    size_t header_size = mem_header_size();
    size_t node_size = mem_node_size();
    mem_header_ = reinterpret_cast<MemHeader*>(mem);
    if(!is_raw)
    {// 做一下简单的校验
        if(mem_header_->magic_num != MAGIC_NUM || mem_header_->mem_size != mem_size
            || mem_header_->header_size != header_size || mem_header_->node_size != node_size)
        {
            err_msg_ = "mem header check err";
            return false;
        }
    }
    else
    {// 初始化内存头
        memset(mem_header_, 0, header_size);
        
        mem_header_->magic_num = MAGIC_NUM;
        mem_header_->mem_size = mem_size;
        mem_header_->alloc_size = header_size;
        mem_header_->header_size = header_size;
        mem_header_->node_size = node_size;
        mem_header_->free_list = 0;

        size_t node_ref = alloc_node();
        mem_header_->sl_info.head = node_ref;
        mem_header_->sl_info.tail = 0;
        mem_header_->sl_info.level_num = 1;
        mem_header_->sl_info.length = 0;

        //初始化跳跃表的头节点
        MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
        node->sl_node_info.backword = 0;
        for(int i = 0; i < MAX_LEVEL_NUM; ++i)
        {
            node->sl_node_info.level[i].forward = 0;
            node->sl_node_info.level[i].span = 0;
        }
    }

    return true;
}

template<typename T, typename Compare>
uint32_t SkipList<T, Compare>::get_index(const T& element) const
{
    uint32_t index = 0;
    MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
    Compare cmp;
    for(int i = mem_header_->sl_info.level_num - 1; i >= 0; --i)
    {
        while(node->sl_node_info.level[i].forward)
        {
            MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[i].forward));
            if(cmp(forward_node->sl_node_info.element, element))
            {
                index += node->sl_node_info.level[i].span;
                node = forward_node;
            }
            else
            {
                if(!cmp(element, forward_node->sl_node_info.element))
                    return index + node->sl_node_info.level[i].span;
                
                break;
            }
        }
    }

    return 0;
}

template<typename T, typename Compare>
typename SkipList<T, Compare>::Iterator SkipList<T, Compare>::find(const T& element) const
{
    MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
    Compare cmp;
    for(int i = mem_header_->sl_info.level_num - 1; i >= 0; --i)
    {
        while(node->sl_node_info.level[i].forward)
        {
            MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[i].forward));
            if(cmp(forward_node->sl_node_info.element, element))
            {
                node = forward_node;
            }
            else
            {
                if(!cmp(element, forward_node->sl_node_info.element))
                    return Iterator(this, node->sl_node_info.level[i].forward);

                break;
            }
        }
    }

    return Iterator(this, 0);
}

template<typename T, typename Compare>
typename SkipList<T, Compare>::Iterator SkipList<T, Compare>::find(uint32_t index) const
{
    uint32_t total_span = 0;
    MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
    for(int i = mem_header_->sl_info.level_num - 1; i >= 0; --i)
    {
        while(node->sl_node_info.level[i].forward)
        {
            MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[i].forward));
            if(total_span + node->sl_node_info.level[i].span <= index)
            {
                total_span += node->sl_node_info.level[i].span;
                node = forward_node;
            }
            else
            {
                if(total_span == index && ref(node) != mem_header_->sl_info.head)
                    return Iterator(this, ref(node));

                break;
            }
        }
    }

    return Iterator(this, 0);
}

template<typename T, typename Compare>
bool SkipList<T, Compare>::insert(const T& element)
{
    size_t new_node_ref = alloc_node();
    if(!new_node_ref) return false;

    uint32_t index[MAX_LEVEL_NUM] = {0};
    size_t update[MAX_LEVEL_NUM] = {0};
    MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
    Compare cmp;
    for(int i = mem_header_->sl_info.level_num - 1; i >= 0; --i)
    {
        index[i] = i == mem_header_->sl_info.level_num - 1 ? 0 : index[i + 1];
        while(node->sl_node_info.level[i].forward)
        {
            MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[i].forward));
            if(cmp(forward_node->sl_node_info.element, element))
            {
                index[i] += node->sl_node_info.level[i].span;
                node = forward_node;
            }
            else
                break;
        }
        update[i] = ref(node);
    }

    int level = random_level();
    if(level > mem_header_->sl_info.level_num)
    {
        for(int i = mem_header_->sl_info.level_num; i < level; ++i)
        {
            index[i] = 0;
            update[i] = mem_header_->sl_info.head;
            MemNode* update_node = reinterpret_cast<MemNode*>(deref(update[i]));
            update_node->sl_node_info.level[i].span = mem_header_->sl_info.length;
        }
        mem_header_->sl_info.level_num = level;
    }

    MemNode* new_node = reinterpret_cast<MemNode*>(deref(new_node_ref));
    for(int i = 0; i < level; ++i)
    {
        MemNode* update_node = reinterpret_cast<MemNode*>(deref(update[i]));
        new_node->sl_node_info.level[i].forward = update_node->sl_node_info.level[i].forward;
        update_node->sl_node_info.level[i].forward = new_node_ref;
        new_node->sl_node_info.level[i].span = update_node->sl_node_info.level[i].span - (index[0] - index[i]);
        update_node->sl_node_info.level[i].span = index[0] - index[i] + 1; 
    }

    for(int i = level; i < mem_header_->sl_info.level_num; ++i)
    {
        MemNode* update_node = reinterpret_cast<MemNode*>(deref(update[i]));
        update_node->sl_node_info.level[i].span += 1;
    }

    if(update[0] == mem_header_->sl_info.head)
        new_node->sl_node_info.backword = 0;
    else
        new_node->sl_node_info.backword = update[0];
    
    if(new_node->sl_node_info.level[0].forward)
    {
        MemNode* forward_node = reinterpret_cast<MemNode*>(deref(new_node->sl_node_info.level[0].forward));
        forward_node->sl_node_info.backword = new_node_ref;
    }
    else
        mem_header_->sl_info.tail = new_node_ref;
    
    new_node->sl_node_info.element = element;
    mem_header_->sl_info.length += 1;

    return true;
}

template<typename T, typename Compare>
uint32_t SkipList<T, Compare>::erase(const T& element)
{
    uint32_t count = 0;
    while(del_first_of(element))
        ++count;
    
    return count;
}

template<typename T, typename Compare>
bool SkipList<T, Compare>::del_first_of(const T& element)
{
    size_t update[MAX_LEVEL_NUM] = {0};
    MemNode* node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
    Compare cmp;
    for(int i = mem_header_->sl_info.level_num - 1; i >= 0; --i)
    {
        while(node->sl_node_info.level[i].forward)
        {
            MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[i].forward));
            if(cmp(forward_node->sl_node_info.element, element))
                node = forward_node;
            else
                break;
        }
        update[i] = ref(node);
    }

    if(node->sl_node_info.level[0].forward)
    {
        MemNode* forward_node = reinterpret_cast<MemNode*>(deref(node->sl_node_info.level[0].forward));
        if(!cmp(element, forward_node->sl_node_info.element)
            && !cmp(forward_node->sl_node_info.element, element))
        {
            MemNode* del_node = forward_node;
            size_t del_node_ref = ref(del_node);
            for(int i = 0; i < mem_header_->sl_info.level_num; ++i)
            {
                MemNode* update_node = reinterpret_cast<MemNode*>(deref(update[i]));
                if(update_node->sl_node_info.level[i].forward == del_node_ref)
                {
                    update_node->sl_node_info.level[i].span += del_node->sl_node_info.level[i].span;
                    update_node->sl_node_info.level[i].span -= 1;
                    update_node->sl_node_info.level[i].forward = del_node->sl_node_info.level[i].forward;
                }
                else
                {
                    update_node->sl_node_info.level[i].span -= 1;
                }
            }

            if(del_node->sl_node_info.level[0].forward)
            {
                forward_node = reinterpret_cast<MemNode*>(deref(del_node->sl_node_info.level[0].forward));
                forward_node->sl_node_info.backword = del_node->sl_node_info.backword;
            }
            else
                mem_header_->sl_info.tail = del_node->sl_node_info.backword;
            
            node = reinterpret_cast<MemNode*>(deref(mem_header_->sl_info.head));
            while(mem_header_->sl_info.level_num > 1 
                && node->sl_node_info.level[mem_header_->sl_info.level_num - 1].forward == 0)
            {
                mem_header_->sl_info.level_num -= 1;
            }
            
            mem_header_->sl_info.length -= 1;
            free_node(del_node_ref);

            return true;
        }
    }

    return false;
}

template<typename T, typename Compare>
size_t SkipList<T, Compare>::alloc_node()
{
    size_t pos = 0;
    MemNode* node = nullptr;
    if(mem_header_->free_list)
    {// 空闲链表非空，从空闲链表上申请节点
        node = reinterpret_cast<MemNode*>(deref(mem_header_->free_list));
        pos = mem_header_->free_list;
        mem_header_->free_list = node->next;
    }
    else if(mem_header_->alloc_size + mem_header_->node_size <= mem_header_->mem_size)
    {// 空闲链表为空且还有空间，从未使用的内存中申请节点
        node = reinterpret_cast<MemNode*>(deref(mem_header_->alloc_size));
        pos = mem_header_->alloc_size;
        mem_header_->alloc_size += mem_header_->node_size;
    }

    if(node) memset(node, 0, mem_header_->node_size);

    return pos;
}

template<typename T, typename Compare>
void SkipList<T, Compare>::free_node(size_t node_ref)
{
    MemNode* node = reinterpret_cast<MemNode*>(deref(node_ref));
    node->next = mem_header_->free_list;
    mem_header_->free_list = node_ref;
}
