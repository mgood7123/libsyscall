#include <libsyscall/wl_fd_allocator.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

bool wl_miniobj_debug = false;

#include <stdio.h>

// INTEGER FILE DESCRIPTOR ALLOCATOR

#include <cstdbool> // bool
#include <cstdio>   // printf
#include <cstring>  // malloc
#include <vector>   // vector

static void WL_SYSCALLS_FD_ALLOCATOR__DESTROY_DATA_CALLBACK__DO_NOTHING(int index, void** unused, bool in_destructor) {}

class ShrinkingVectorIndexAllocator {
public:
    class Holder {
    public:

        bool used;
        void* data;
        int index;
        WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback;

        Holder(void* data, size_t index, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback)
            : used(false), data(data), index((int)index), callback(callback == nullptr ? WL_SYSCALLS_FD_ALLOCATOR__DESTROY_DATA_CALLBACK__DO_NOTHING : callback)
        {}

        Holder(void) : Holder(nullptr, 0, WL_SYSCALLS_FD_ALLOCATOR__DESTROY_DATA_CALLBACK__DO_NOTHING)
        {}

        void destroy(void) {
            if (callback != NULL) {
                callback(index, &data, false);
                callback = NULL;
            }
        }

        ~Holder(void) {
            if (callback != NULL) {
                callback(index, &data, true);
                callback = NULL;
            }
        }
    };

    class Chunk {
    public:

        Holder* data;
        size_t size;
        size_t capacity;

        Chunk(Holder* data, size_t size, size_t capacity)
            :
            data(data),
            size(size),
            capacity(capacity)
        {};

        Chunk(Chunk&& old) {
            data = old.data;
            size = old.size;
            capacity = old.capacity;
            old.data = nullptr;
            old.size = 0;
            old.capacity = 0;
        }

        Chunk& operator=(Chunk&& old) {
            data = old.data;
            size = old.size;
            capacity = old.capacity;
            old.data = nullptr;
            old.size = 0;
            old.capacity = 0;
            return *this;
        }

        ~Chunk(void) {
            delete[] data;
        }
    };

    std::vector<Chunk> chunks;
    size_t current_chunk_index;
    size_t total_size;
    size_t total_capacity;

    int get_chunk(size_t i) {
        return (8 * sizeof(size_t) - __builtin_clzll(i + 2) - 1) - 1;
    }

    size_t get_chunk_subindex(size_t i, int chunk) {
        return i - (1 << (chunk + 1)) + 2;
    }

    void print_chunk_and_index(size_t i) {
        int chunk = get_chunk(i);
        printf("index: %zu, chunk: %d, sub array index: %zu\n", i, chunk, get_chunk_subindex(i, chunk));
    }

public:
    ShrinkingVectorIndexAllocator(void) {
        chunks = std::vector<Chunk>();
        current_chunk_index = 0;
        total_size = 0;
        total_capacity = 0;
    }
    size_t size(void) { return total_size; }
    size_t capacity(void) { return total_capacity; }
    void* operator[] (size_t value) {
        int CI = get_chunk(value);
        size_t DI = get_chunk_subindex(value, CI);
        return chunks[CI].data[DI].data;
    }

    void print(void) {
        if (total_capacity == 0) {
            printf("array = nullptr\n");
        }
        else {
            printf("array (size %zu, capacity %zu, chunk size: %d, chunk capacity: %d)\n", total_size, total_capacity, (int)chunks.size(), (int)chunks.capacity());
            for (int i = 0; i < chunks.size(); i++) {
                printf("  chunk %d (size %zu, capacity %zu) = [", i, chunks[i].size, chunks[i].capacity);
                for (size_t i_ = 0; i_ < chunks[i].capacity; i_++) {
                    if (i_ != 0) printf(", ");
                    printf("%zu", i_);
                }
                printf("]\n");
            }
        }
    }

    size_t add(void* value, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback) {
        if (total_capacity == 0) {
            chunks.reserve(1);
            chunks.emplace_back(Chunk(new Holder[2], 1, 2));
            chunks[0].data[0].used = true;
            chunks[0].data[0].data = value;
            chunks[0].data[0].index = total_size;
            chunks[0].data[0].callback = callback;
            total_size++;
            total_capacity += 2;
            current_chunk_index = 0;
            if (wl_miniobj_debug) printf("added, total_size: %zu, total_capacity: %zu\n", total_size, total_capacity);
            return total_size - 1;
        }
        else {
            if (chunks[current_chunk_index].size == chunks[current_chunk_index].capacity) {
                size_t cap = chunks[current_chunk_index].capacity * 2;
                chunks.reserve(current_chunk_index + 2);
                chunks.emplace_back(Chunk(new Holder[cap], 0, cap));
                current_chunk_index++;
                chunks[current_chunk_index].data[0].used = true;
                chunks[current_chunk_index].data[0].data = value;
                chunks[current_chunk_index].data[0].index = total_size;
                chunks[current_chunk_index].data[0].callback = callback;
                chunks[current_chunk_index].size = 1;
                total_size++;
                total_capacity += chunks[current_chunk_index].capacity;
                if (wl_miniobj_debug) printf("added, total_size: %zu, total_capacity: %zu\n", total_size, total_capacity);
                return total_size - 1;
            }
            else {
                chunks[current_chunk_index].data[chunks[current_chunk_index].size].used = true;
                chunks[current_chunk_index].data[chunks[current_chunk_index].size].data = value;
                chunks[current_chunk_index].data[chunks[current_chunk_index].size].index = total_size;
                chunks[current_chunk_index].data[chunks[current_chunk_index].size].callback = callback;
                chunks[current_chunk_index].size++;
                total_size++;
                if (wl_miniobj_debug) printf("added, total_size: %zu, total_capacity: %zu\n", total_size, total_capacity);
                return total_size - 1;
            }
        }
    }

    size_t reuse(size_t index, void* data, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback) {
        if (total_capacity == 0) {
            return -1;
        }
        int CI = get_chunk(index);
        size_t DI = get_chunk_subindex(index, CI);
        chunks[CI].data[DI].used = true;
        chunks[CI].data[DI].data = data;
        chunks[CI].data[DI].callback = callback;
        chunks[CI].size++;
        total_size++;
        if (wl_miniobj_debug) printf("reuse, total_size: %zu, total_capacity: %zu\n", total_size, total_capacity);
        return index;
    }

    Holder* index_if_valid(size_t index, int* CI, size_t* DI) {
        if (total_capacity == 0) {
            if (wl_miniobj_debug) printf("index invalid: %zu (total capazity is zero), false\n", index);
            return NULL;
        }
        *CI = get_chunk(index);
        size_t s = chunks.size();
        if (*CI >= s) {
            if (wl_miniobj_debug) printf("index invalid: %zu (CI %i >= %zu), false\n", index, *CI, s);
            return NULL;
        }
        *DI = get_chunk_subindex(index, *CI);
        if (*DI >= chunks[*CI].capacity) {
            if (wl_miniobj_debug) printf("index invalid: %zu (DI %zu >= %zu), false\n", index, *DI, chunks[*CI].capacity);
            return NULL;
        }
        if (chunks[*CI].data[*DI].used) {
            if (wl_miniobj_debug) printf("index valid: %zu, true\n", index);
            return &chunks[*CI].data[*DI];
        }
        if (wl_miniobj_debug) printf("index invalid: %zu, false\n", index);
        return NULL;
    }

    bool remove(size_t index) {
        if (total_capacity == 0) {
            return false;
        }
        int CI;
        size_t DI;
        Holder* holder = index_if_valid(index, &CI, &DI);
        if (holder == NULL) {
            return false;
        }
        if (wl_miniobj_debug) printf("removing %zu, CI: %d, DI: %zu, total_size: %zu, total_capacity: %zu, chunks size: %zu\n", index, CI, DI, total_size, total_capacity, chunks.size());
        chunks[CI].data[DI].destroy();
        chunks[CI].data[DI].used = false;
        chunks[CI].size--;
        total_size--;
        if (chunks[CI].size == 0) {
            total_capacity -= chunks[CI].capacity;
            if (total_capacity == 0) {
                chunks.clear();
                chunks.shrink_to_fit();
            }
            else {
                if (chunks[chunks.size() - 1].size == 0) {
                    int CII = chunks.size() - 1;
                    while (true) {
                        if (chunks[CII].size != 0) {
                            break;
                        }
                        current_chunk_index--;
                        chunks.erase(chunks.cbegin() + CII);
                        chunks.shrink_to_fit();
                        CII--;
                    }
                }
            }
        }
        return true;
    }
};

// INTEGER FILE DESCRIPTOR RECYCLER

// C++ needed for implementation

// this files contains all the application independent little
// functions and macros used for the optimizer.
// In particular Peters debug macros and Dags stuff
// from dbasic.h cdefs, random,...

//////////////// stuff originally from debug.h ///////////////////////////////
// (c) 1997 Peter Sanders
// some little utilities for debugging adapted
// to the paros conventions


#ifndef UTIL
#define UTIL

#define Assert(c)
#define Assert2(c)
#define Debug4(c)

////////////// min, max etc. //////////////////////////////////////

#ifndef Max
#define Max(x,y) ((x)>=(y)?(x):(y))
#endif

#ifndef Min
#define Min(x,y) ((x)<=(y)?(x):(y))
#endif

#ifndef Abs
#define Abs(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef PI
#define PI 3.1415927
#endif

// is this the right definition of limit?
inline double limit(double x, double bound)
{
    if (x > bound) { return  bound; }
    else if (x < -bound) { return -bound; }
    else                   return x;
}

#endif

// hierarchical memory priority queue data structure
#ifndef KNHEAP
#define KNHEAP

const int KNBufferSize1 = 32; // equalize procedure call overheads etc.
const int KNN = 512; // bandwidth
const int KNKMAX = 64;  // maximal arity
const int KNLevels = 4; // overall capacity >= KNN*KNKMAX^KNLevels
/*
const int KNBufferSize1 = 3; // equalize procedure call overheads etc.
const int KNN = 10; // bandwidth
const int KNKMAX = 4;  // maximal arity
const int KNLevels = 4; // overall capacity >= KNN*KNKMAX^KNLevels
*/
template <class Key, class Value>
struct KNElement { Key key; Value value; };

//////////////////////////////////////////////////////////////////////
// fixed size binary heap
template <class Key, class Value, int capacity>
class BinaryHeap {
    //  static const Key infimum  = 4;
    //static const Key supremum = numeric_limits<Key>.max();
    typedef KNElement<Key, Value> Element;
    Element data[capacity + 2];
    int size;  // index of last used element
public:
    BinaryHeap(Key sup, Key infimum) :size(0) {
        data[0].key = infimum; // sentinel
        data[capacity + 1].key = sup;
        reset();
    }
    Key getSupremum(void) { return data[capacity + 1].key; }
    void reset(void);
    int   getSize(void)     const { return size; }
    Key   getMinKey(void)   const { return data[1].key; }
    Value getMinValue(void) const { return data[1].value; }
    void  deleteMin(void);
    void  deleteMinFancy(Key* key, Value* value) {
        *key = getMinKey();
        *value = getMinValue();
        deleteMin();
    }
    void  insert(Key k, Value v);
    void  sortTo(Element* to); // sort in increasing order and empty
    //void  sortInPlace(void); // in decreasing order
};


// reset size to 0 and fill data array with sentinels
template <class Key, class Value, int capacity>
inline void BinaryHeap<Key, Value, capacity>::
reset(void) {
    size = 0;
    Key sup = getSupremum();
    for (int i = 1; i <= capacity; i++) {
        data[i].key = sup;
    }
    // if this becomes a bottle neck
    // we might want to replace this by log KNN
    // memcpy-s
}

template <class Key, class Value, int capacity>
inline void BinaryHeap<Key, Value, capacity>::
deleteMin(void)
{
    Assert2(size > 0);

    // first move up elements on a min-path
    int hole = 1;
    int succ = 2;
    int sz = size;
    while (succ < sz) {
        Key key1 = data[succ].key;
        Key key2 = data[succ + 1].key;
        if (key1 > key2) {
            succ++;
            data[hole].key = key2;
            data[hole].value = data[succ].value;
        }
        else {
            data[hole].key = key1;
            data[hole].value = data[succ].value;
        }
        hole = succ;
        succ <<= 1;
    }

    // bubble up rightmost element
    Key bubble = data[sz].key;
    int pred = hole >> 1;
    while (data[pred].key > bubble) { // must terminate since min at root
        data[hole] = data[pred];
        hole = pred;
        pred >>= 1;
    }

    // finally move data to hole
    data[hole].key = bubble;
    data[hole].value = data[sz].value;

    data[size].key = getSupremum(); // mark as deleted
    size = sz - 1;
}


// empty the heap and put the element to "to"
// sorted in increasing order
template <class Key, class Value, int capacity>
inline void BinaryHeap<Key, Value, capacity>::
sortTo(Element* to)
{
    const int           sz = size;
    const Key          sup = getSupremum();
    Element* const beyond = to + sz;
    Element* const root = data + 1;
    while (to < beyond) {
        // copy minimun
        *to = *root;
        to++;

        // bubble up second smallest as in deleteMin
        int hole = 1;
        int succ = 2;
        while (succ <= sz) {
            Key key1 = data[succ].key;
            Key key2 = data[succ + 1].key;
            if (key1 > key2) {
                succ++;
                data[hole].key = key2;
                data[hole].value = data[succ].value;
            }
            else {
                data[hole].key = key1;
                data[hole].value = data[succ].value;
            }
            hole = succ;
            succ <<= 1;
        }

        // just mark hole as deleted
        data[hole].key = sup;
    }
    size = 0;
}


template <class Key, class Value, int capacity>
inline void BinaryHeap<Key, Value, capacity>::
insert(Key k, Value v)
{
    Assert2(size < capacity);

    size++;
    int hole = size;
    int pred = hole >> 1;
    Key predKey = data[pred].key;
    while (predKey > k) { // must terminate due to sentinel at 0
        data[hole].key = predKey;
        data[hole].value = data[pred].value;
        hole = pred;
        pred >>= 1;
        predKey = data[pred].key;
    }

    // finally move data to hole
    data[hole].key = k;
    data[hole].value = v;
}

//////////////////////////////////////////////////////////////////////
// The data structure from Knuth, "Sorting and Searching", Section 5.4.1
template <class Key, class Value>
class KNLooserTree {
    // public: // should not be here but then I would need a scary
    // sequence of template friends which I doubt to work
    // on all compilers
    typedef KNElement<Key, Value> Element;
    struct Entry {
        Key key;   // Key of Looser element (winner for 0)
        int index; // number of loosing segment
    };

    // stack of empty segments
    int empty[KNKMAX]; // indices of empty segments
    int lastFree;  // where in "empty" is the last valid entry?

    int size; // total number of elements stored
    int logK; // log of current tree size
    int k; // invariant k = 1 << logK

    Element dummy; // target of empty segment pointers

    // upper levels of looser trees
    // entry[0] contains the winner info
    Entry entry[KNKMAX];

    // leaf information
    // note that Knuth uses indices k..k-1
    // while we use 0..k-1
    Element* current[KNKMAX]; // pointer to actual element
    Element* segment[KNKMAX]; // start of Segments

    // private member functions
    int initWinner(int root);
    void updateOnInsert(int node, Key newKey, int newIndex,
        Key* winnerKey, int* winnerIndex, int* mask);
    void deallocateSegment(int index);
    void doubleK(void);
    void compactTree(void);
    void rebuildLooserTree(void);
    int segmentIsEmpty(int i);
public:
    KNLooserTree(void);
    void init(Key sup); // before, no consistent state is reached :-(

    void multiMergeUnrolled3(Element* to, int l);
    void multiMergeUnrolled4(Element* to, int l);
    void multiMergeUnrolled5(Element* to, int l);
    void multiMergeUnrolled6(Element* to, int l);
    void multiMergeUnrolled7(Element* to, int l);
    void multiMergeUnrolled8(Element* to, int l);
    void multiMergeUnrolled9(Element* to, int l);
    void multiMergeUnrolled10(Element* to, int l);

    void multiMerge(Element* to, int l); // delete l smallest element to "to"
    void multiMergeK(Element* to, int l);
    int  spaceIsAvailable(void) { return k < KNKMAX || lastFree >= 0; }
    // for new segment
    void insertSegment(Element* to, int sz); // insert segment beginning at to
    int  getSize(void) { return size; }
    Key getSupremum(void) { return dummy.key; }
};


//////////////////////////////////////////////////////////////////////
// 2 level multi-merge tree
template <class Key, class Value>
class KNHeap {
    typedef KNElement<Key, Value> Element;

    KNLooserTree<Key, Value> tree[KNLevels];

    // one delete buffer for each tree (extra space for sentinel)
    Element buffer2[KNLevels][KNN + 1]; // tree->buffer2->buffer1
    Element* minBuffer2[KNLevels];

    // overall delete buffer
    Element buffer1[KNBufferSize1 + 1];
    Element* minBuffer1;

    // insert buffer
    BinaryHeap<Key, Value, KNN> insertHeap;

    // how many levels are active
    int activeLevels;

    // total size not counting insertBuffer and buffer1
    int size;

    // private member functions
    void refillBuffer1(void);
    void refillBuffer11(int sz);
    void refillBuffer12(int sz);
    void refillBuffer13(int sz);
    void refillBuffer14(int sz);
    int refillBuffer2(int k);
    int makeSpaceAvailable(int level);
    void emptyInsertHeap(void);
    Key getSupremum(void) const { return buffer2[0][KNN].key; }
    int getSize1(void) const { return (buffer1 + KNBufferSize1) - minBuffer1; }
    int getSize2(int i) const { return &(buffer2[i][KNN]) - minBuffer2[i]; }
public:
    KNHeap(Key sup, Key infimum);
    int   getSize(void) const;
    void  getMin(Key* key, Value* value);
    void  deleteMin(Key* key, Value* value);
    void  insert(Key key, Value value);
};


template <class Key, class Value>
inline int KNHeap<Key, Value>::getSize(void) const
{
    return
        size +
        insertHeap.getSize() +
        ((buffer1 + KNBufferSize1) - minBuffer1);
}

template <class Key, class Value>
inline void  KNHeap<Key, Value>::getMin(Key* key, Value* value) {
    Key key1 = minBuffer1->key;
    Key key2 = insertHeap.getMinKey();
    if (key2 >= key1) {
        *key = key1;
        *value = minBuffer1->value;
    }
    else {
        *key = key2;
        *value = insertHeap.getMinValue();
    }
}

template <class Key, class Value>
inline void  KNHeap<Key, Value>::deleteMin(Key* key, Value* value) {
    Key key1 = minBuffer1->key;
    Key key2 = insertHeap.getMinKey();
    if (key2 >= key1) {
        *key = key1;
        *value = minBuffer1->value;
        Assert2(minBuffer1 < buffer1 + KNBufferSize1); // no delete from empty
        minBuffer1++;
        if (minBuffer1 == buffer1 + KNBufferSize1) {
            refillBuffer1();
        }
    }
    else {
        *key = key2;
        *value = insertHeap.getMinValue();
        insertHeap.deleteMin();
    }
}

template <class Key, class Value>
inline  void  KNHeap<Key, Value>::insert(Key k, Value v) {
    if (insertHeap.getSize() == KNN) { emptyInsertHeap(); }
    insertHeap.insert(k, v);
}
#endif

#include <string.h>
///////////////////////// LooserTree ///////////////////////////////////
template <class Key, class Value>
KNLooserTree<Key, Value>::
KNLooserTree(void) : lastFree(0), size(0), logK(0), k(1)
{
    empty[0] = 0;
    segment[0] = 0;
    current[0] = &dummy;
    // entry and dummy are initialized by init
    // since they need the value of supremum
}


template <class Key, class Value>
void KNLooserTree<Key, Value>::
init(Key sup)
{
    dummy.key = sup;
    rebuildLooserTree();
    Assert2(current[entry[0].index] == &dummy);
}


// rebuild looser tree information from the values in current
template <class Key, class Value>
void KNLooserTree<Key, Value>::
rebuildLooserTree(void)
{
    int winner = initWinner(1);
    entry[0].index = winner;
    entry[0].key = current[winner]->key;
}


// given any values in the leaves this
// routing recomputes upper levels of the tree
// from scratch in linear time
// initialize entry[root].index and the subtree rooted there
// return winner index
template <class Key, class Value>
int KNLooserTree<Key, Value>::
initWinner(int root)
{
    if (root >= k) { // leaf reached
        return root - k;
    }
    else {
        int left = initWinner(2 * root);
        int right = initWinner(2 * root + 1);
        Key lk = current[left]->key;
        Key rk = current[right]->key;
        if (lk <= rk) { // right subtree looses
            entry[root].index = right;
            entry[root].key = rk;
            return left;
        }
        else {
            entry[root].index = left;
            entry[root].key = lk;
            return right;
        }
    }
}


// first go up the tree all the way to the root
// hand down old winner for the respective subtree
// based on new value, and old winner and looser 
// update each node on the path to the root top down.
// This is implemented recursively
template <class Key, class Value>
void KNLooserTree<Key, Value>::
updateOnInsert(int node,
    Key     newKey, int     newIndex,
    Key* winnerKey, int* winnerIndex, // old winner
    int* mask) // 1 << (ceil(log KNK) - dist-from-root)
{
    if (node == 0) { // winner part of root
        *mask = 1 << (logK - 1);
        *winnerKey = entry[0].key;
        *winnerIndex = entry[0].index;
        if (newKey < entry[node].key) {
            entry[node].key = newKey;
            entry[node].index = newIndex;
        }
    }
    else {
        updateOnInsert(node >> 1, newKey, newIndex, winnerKey, winnerIndex, mask);
        Key looserKey = entry[node].key;
        int looserIndex = entry[node].index;
        if ((*winnerIndex & *mask) != (newIndex & *mask)) { // different subtrees
            if (newKey < looserKey) { // newKey will have influence here
                if (newKey < *winnerKey) { // old winner loses here
                    entry[node].key = *winnerKey;
                    entry[node].index = *winnerIndex;
                }
                else { // new entry looses here
                    entry[node].key = newKey;
                    entry[node].index = newIndex;
                }
            }
            *winnerKey = looserKey;
            *winnerIndex = looserIndex;
        }
        // note that nothing needs to be done if
        // the winner came from the same subtree
        // a) newKey <= winnerKey => even more reason for the other tree to loose
        // b) newKey >  winnerKey => the old winner will beat the new
        //                           entry further down the tree
        // also the same old winner is handed down the tree

        *mask >>= 1; // next level
    }
}


// make the tree two times as wide
// may only be called if no free slots are left ?? necessary ??
template <class Key, class Value>
void KNLooserTree<Key, Value>::
doubleK(void)
{
    // make all new entries empty
    // and push them on the free stack
    Assert2(lastFree == -1); // stack was empty (probably not needed)
    Assert2(k < KNKMAX);
    for (int i = 2 * k - 1; i >= k; i--) {
        current[i] = &dummy;
        lastFree++;
        empty[lastFree] = i;
    }

    // double the size
    k *= 2;  logK++;

    // recompute looser tree information
    rebuildLooserTree();
}


// compact nonempty segments in the left half of the tree
template <class Key, class Value>
void KNLooserTree<Key, Value>::
compactTree(void)
{
    Assert2(logK > 0);
    Key sup = dummy.key;

    // compact all nonempty segments to the left
    int from = 0;
    int to = 0;
    for (; from < k; from++) {
        if (current[from]->key != sup) {
            current[to] = current[from];
            segment[to] = segment[from];
            to++;
        }
    }

    // half degree as often as possible
    while (to < k / 2) {
        k /= 2;  logK--;
    }

    // overwrite garbage and compact the stack of empty segments
    lastFree = -1; // none free
    for (; to < k; to++) {
        // push 
        lastFree++;
        empty[lastFree] = to;

        current[to] = &dummy;
    }

    // recompute looser tree information
    rebuildLooserTree();
}


// insert segment beginning at to
// require: spaceIsAvailable() == 1 
template <class Key, class Value>
void KNLooserTree<Key, Value>::
insertSegment(Element* to, int sz)
{
    if (sz > 0) {
        Assert2(to[0].key != getSupremum());
        Assert2(to[sz - 1].key != getSupremum());
        // get a free slot
        if (lastFree < 0) { // tree is too small
            doubleK();
        }
        int index = empty[lastFree];
        lastFree--; // pop


        // link new segment
        current[index] = segment[index] = to;
        size += sz;

        // propagate new information up the tree
        Key dummyKey;
        int dummyIndex;
        int dummyMask;
        updateOnInsert((index + k) >> 1, to->key, index,
            &dummyKey, &dummyIndex, &dummyMask);
    }
    else {
        // immediately deallocate
        // this is not only an optimization 
        // but also needed to keep empty segments from
        // clogging up the tree
        delete[] to;
    }
}


// free an empty segment
template <class Key, class Value>
void KNLooserTree<Key, Value>::
deallocateSegment(int index)
{
    // reroute current pointer to some empty dummy segment
    // with a sentinel key
    current[index] = &dummy;

    // free memory
    delete[] segment[index];
    segment[index] = 0;

    // push on the stack of free segment indices
    lastFree++;
    empty[lastFree] = index;
}

// multi-merge for a fixed K=1<<LogK
// this looks ugly but the include file explains
// why this is the most portable choice
#define LogK 3
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled3(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 4
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled4(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 5
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled5(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 6
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled6(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 7
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled7(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 8
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled8(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 9
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled9(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK
#define LogK 10
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeUnrolled10(Element* to, int l)
// body of the function multiMergeUnrolled
// it will be included multiple times with
// different settings for LogP
// Note that in gcc it is sufficient to simply
// use an addtional argument and to declare things an inline
// function. But I was not able to convince SunCC to
// inline and constant fold it.
// Similarly I tried introducing LogP as a template
// parameter but this did not compile on SunCC
{
    Element* done = to + l;
    Entry* regEntry = entry;
    Element** regCurrent = current;
    int      winnerIndex = regEntry[0].index;
    Key      winnerKey = regEntry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum

    Assert2(logK >= LogK);
    while (to < done) {
        winnerPos = regCurrent[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        regCurrent[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }
        to++;

        // update looser tree
#define TreeStep(L)\
      if (1 << LogK >= 1 << L) {\
        Entry *pos##L = regEntry+((winnerIndex+(1<<LogK)) >> ((LogK-L)+1));\
        Key    key##L = pos##L->key;\
        if (key##L < winnerKey) {\
          int index##L  = pos##L->index;\
          pos##L->key   = winnerKey;\
          pos##L->index = winnerIndex;\
          winnerKey     = key##L;\
          winnerIndex   = index##L;\
        }\
      }
        TreeStep(10);
        TreeStep(9);
        TreeStep(8);
        TreeStep(7);
        TreeStep(6);
        TreeStep(5);
        TreeStep(4);
        TreeStep(3);
        TreeStep(2);
        TreeStep(1);
#undef TreeStep      
    }
    regEntry[0].index = winnerIndex;
    regEntry[0].key = winnerKey;
}
#undef LogK

// delete the l smallest elements and write them to "to"
// empty segments are deallocated
// require:
// - there are at least l elements
// - segments are ended by sentinels
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMerge(Element* to, int l)
{
    switch (logK) {
    case 0:
        Assert2(k == 1);
        Assert2(entry[0].index == 0);
        Assert2(lastFree == -1 || l == 0);
        memcpy(to, current[0], l * sizeof(Element));
        current[0] += l;
        entry[0].key = current[0]->key;
        if (segmentIsEmpty(0)) deallocateSegment(0);
        break;
    case 1:
        Assert2(k == 2);
        merge(current + 0, current + 1, to, l);
        rebuildLooserTree();
        if (segmentIsEmpty(0)) deallocateSegment(0);
        if (segmentIsEmpty(1)) deallocateSegment(1);
        break;
    case 2:
        Assert2(k == 4);
        merge4(current + 0, current + 1, current + 2, current + 3, to, l);
        rebuildLooserTree();
        if (segmentIsEmpty(0)) deallocateSegment(0);
        if (segmentIsEmpty(1)) deallocateSegment(1);
        if (segmentIsEmpty(2)) deallocateSegment(2);
        if (segmentIsEmpty(3)) deallocateSegment(3);
        break;
    case  3: multiMergeUnrolled3(to, l); break;
    case  4: multiMergeUnrolled4(to, l); break;
    case  5: multiMergeUnrolled5(to, l); break;
    case  6: multiMergeUnrolled6(to, l); break;
    case  7: multiMergeUnrolled7(to, l); break;
    case  8: multiMergeUnrolled8(to, l); break;
    case  9: multiMergeUnrolled9(to, l); break;
    case 10: multiMergeUnrolled10(to, l); break;
    default: multiMergeK(to, l); break;
    }
    size -= l;

    // compact tree if it got considerably smaller
    if (k > 1 && lastFree >= 3 * k / 5 - 1) {
        // using k/2 would be worst case inefficient
        compactTree();
    }
}


// is this segment empty and does not point to dummy yet?
template <class Key, class Value>
inline int KNLooserTree<Key, Value>::
segmentIsEmpty(int i)
{
    return current[i]->key == getSupremum() &&
        current[i] != &dummy;
}


// multi-merge for arbitrary K
template <class Key, class Value>
void KNLooserTree<Key, Value>::
multiMergeK(Element* to, int l)
{
    Entry* currentPos;
    Key currentKey;
    int currentIndex; // leaf pointed to by current entry
    int kReg = k;
    Element* done = to + l;
    int      winnerIndex = entry[0].index;
    Key      winnerKey = entry[0].key;
    Element* winnerPos;
    Key sup = dummy.key; // supremum
    while (to < done) {
        winnerPos = current[winnerIndex];

        // write result
        to->key = winnerKey;
        to->value = winnerPos->value;

        // advance winner segment
        winnerPos++;
        current[winnerIndex] = winnerPos;
        winnerKey = winnerPos->key;

        // remove winner segment if empty now
        if (winnerKey == sup) {
            deallocateSegment(winnerIndex);
        }

        // go up the entry-tree
        for (int i = (winnerIndex + kReg) >> 1; i > 0; i >>= 1) {
            currentPos = entry + i;
            currentKey = currentPos->key;
            if (currentKey < winnerKey) {
                currentIndex = currentPos->index;
                currentPos->key = winnerKey;
                currentPos->index = winnerIndex;
                winnerKey = currentKey;
                winnerIndex = currentIndex;
            }
        }

        to++;
    }
    entry[0].index = winnerIndex;
    entry[0].key = winnerKey;
}

////////////////////////// KNHeap //////////////////////////////////////
template <class Key, class Value>
KNHeap<Key, Value>::
KNHeap(Key sup, Key infimum) : insertHeap(sup, infimum),
activeLevels(0), size(0)
{
    buffer1[KNBufferSize1].key = sup; // sentinel
    minBuffer1 = buffer1 + KNBufferSize1; // empty
    for (int i = 0; i < KNLevels; i++) {
        tree[i].init(sup); // put tree[i] in a consistent state
        buffer2[i][KNN].key = sup; // sentinel
        minBuffer2[i] = &(buffer2[i][KNN]); // empty
    }
}


//--------------------- Buffer refilling -------------------------------

// refill buffer2[j] and return number of elements found
template <class Key, class Value>
int KNHeap<Key, Value>::refillBuffer2(int j)
{
    Element* oldTarget;
    int deleteSize;
    int treeSize = tree[j].getSize();
    int bufferSize = (&(buffer2[j][0]) + KNN) - minBuffer2[j];
    if (treeSize + bufferSize >= KNN) { // buffer will be filled
        oldTarget = &(buffer2[j][0]);
        deleteSize = KNN - bufferSize;
    }
    else {
        oldTarget = &(buffer2[j][0]) + KNN - treeSize - bufferSize;
        deleteSize = treeSize;
    }

    // shift  rest to beginning
    // possible hack:
    // - use memcpy if no overlap
    memmove(oldTarget, minBuffer2[j], bufferSize * sizeof(Element));
    minBuffer2[j] = oldTarget;

    // fill remaining space from tree
    tree[j].multiMerge(oldTarget + bufferSize, deleteSize);
    return deleteSize + bufferSize;
}


// move elements from the 2nd level buffers 
// to the delete buffer
template <class Key, class Value>
void KNHeap<Key, Value>::refillBuffer1(void)
{
    int totalSize = 0;
    int sz;
    for (int i = activeLevels - 1; i >= 0; i--) {
        if ((&(buffer2[i][0]) + KNN) - minBuffer2[i] < KNBufferSize1) {
            sz = refillBuffer2(i);
            // max active level dry now?
            if (sz == 0 && i == activeLevels - 1) { activeLevels--; }
            else { totalSize += sz; }
        }
        else {
            totalSize += KNBufferSize1; // actually only a sufficient lower bound
        }
    }
    if (totalSize >= KNBufferSize1) { // buffer can be filled
        minBuffer1 = buffer1;
        sz = KNBufferSize1; // amount to be copied
        size -= KNBufferSize1; // amount left in buffer2
    }
    else {
        minBuffer1 = buffer1 + KNBufferSize1 - totalSize;
        sz = totalSize;
        Assert2(size == sz); // trees and buffer2 get empty
        size = 0;
    }

    // now call simplified refill routines
    // which can make the assumption that
    // they find all they are asked to find in the buffers
    minBuffer1 = buffer1 + KNBufferSize1 - sz;
    switch (activeLevels) {
    case 1: memcpy(minBuffer1, minBuffer2[0], sz * sizeof(Element));
        minBuffer2[0] += sz;
        break;
    case 2: merge(&(minBuffer2[0]),
        &(minBuffer2[1]), minBuffer1, sz);
        break;
    case 3: merge3(&(minBuffer2[0]),
        &(minBuffer2[1]),
        &(minBuffer2[2]), minBuffer1, sz);
        break;
    case 4: merge4(&(minBuffer2[0]),
        &(minBuffer2[1]),
        &(minBuffer2[2]),
        &(minBuffer2[3]), minBuffer1, sz);
        break;
        //  case 2: refillBuffer12(sz); break;
        //  case 3: refillBuffer13(sz); break;
        //  case 4: refillBuffer14(sz); break;
    }
}


template <class Key, class Value>
void KNHeap<Key, Value>::refillBuffer13(int sz)
{
    Assert(0); // not yet implemented
}

template <class Key, class Value>
void KNHeap<Key, Value>::refillBuffer14(int sz)
{
    Assert(0); // not yet implemented
}


//--------------------------------------------------------------------

// check if space is available on level k and
// empty this level if necessary leading to a recursive call.
// return the level where space was finally available
template <class Key, class Value>
int KNHeap<Key, Value>::makeSpaceAvailable(int level)
{
    int finalLevel;

    Assert2(level <= activeLevels);
    if (level == activeLevels) { activeLevels++; }
    if (tree[level].spaceIsAvailable()) {
        finalLevel = level;
    }
    else {
        finalLevel = makeSpaceAvailable(level + 1);
        int segmentSize = tree[level].getSize();
        Element* newSegment = new Element[segmentSize + 1];
        tree[level].multiMerge(newSegment, segmentSize); // empty this level
        //    tree[level].cleanUp();
        newSegment[segmentSize].key = buffer1[KNBufferSize1].key; // sentinel
        // for queues where size << #inserts
        // it might make sense to stay in this level if
        // segmentSize < alpha * KNN * k^level for some alpha < 1
        tree[level + 1].insertSegment(newSegment, segmentSize);
    }
    return finalLevel;
}


// empty the insert heap into the main data structure
template <class Key, class Value>
void KNHeap<Key, Value>::emptyInsertHeap(void)
{
    const Key sup = getSupremum();

    // build new segment
    Element* newSegment = new Element[KNN + 1];
    Element* newPos = newSegment;

    // put the new data there for now
    insertHeap.sortTo(newSegment);
    newSegment[KNN].key = sup; // sentinel

    // copy the buffer1 and buffer2[0] to temporary storage
    // (the tomporary can be eliminated using some dirty tricks)
    const int tempSize = KNN + KNBufferSize1;
    Element temp[tempSize + 1];
    int sz1 = getSize1();
    int sz2 = getSize2(0);
    Element* pos = temp + tempSize - sz1 - sz2;
    memcpy(pos, minBuffer1, sz1 * sizeof(Element));
    memcpy(pos + sz1, minBuffer2[0], sz2 * sizeof(Element));
    temp[tempSize].key = sup; // sentinel

    // refill buffer1
    // (using more complicated code it could be made somewhat fuller
    // in certein circumstances)
    merge(&pos, &newPos, minBuffer1, sz1);

    // refill buffer2[0]
    // (as above we might want to take the opportunity
    // to make buffer2[0] fuller)
    merge(&pos, &newPos, minBuffer2[0], sz2);

    // merge the rest to the new segment
    // note that merge exactly trips into the footsteps
    // of itself
    merge(&pos, &newPos, newSegment, KNN);

    // and insert it
    int freeLevel = makeSpaceAvailable(0);
    Assert2(freeLevel == 0 || tree[0].getSize() == 0);
    tree[0].insertSegment(newSegment, KNN);

    // get rid of invalid level 2 buffers
    // by inserting them into tree 0 (which is almost empty in this case)
    if (freeLevel > 0) {
        for (int i = freeLevel; i >= 0; i--) { // reverse order not needed 
            // but would allow immediate refill
            newSegment = new Element[getSize2(i) + 1]; // with sentinel
            memcpy(newSegment, minBuffer2[i], (getSize2(i) + 1) * sizeof(Element));
            tree[0].insertSegment(newSegment, getSize2(i));
            minBuffer2[i] = buffer2[i] + KNN; // empty
        }
    }

    // update size
    size += KNN;

    // special case if the tree was empty before
    if (minBuffer1 == buffer1 + KNBufferSize1) { refillBuffer1(); }
}

/////////////////////////////////////////////////////////////////////
// auxiliary functions

// merge sz element from the two sentinel terminated input
// sequences *f0 and *f1 to "to"
// advance *fo and *f1 accordingly.
// require: at least sz nonsentinel elements available in f0, f1
// require: to may overwrite one of the sources as long as
//   *fx + sz is before the end of fx
template <class Key, class Value>
void merge(KNElement<Key, Value>** f0,
    KNElement<Key, Value>** f1,
    KNElement<Key, Value>* to, int sz)
{
    KNElement<Key, Value>* from0 = *f0;
    KNElement<Key, Value>* from1 = *f1;
    KNElement<Key, Value>* done = to + sz;
    Key      key0 = from0->key;
    Key      key1 = from1->key;

    while (to < done) {
        if (key1 <= key0) {
            to->key = key1;
            to->value = from1->value; // note that this may be the same address
            from1++; // nach hinten schieben?
            key1 = from1->key;
        }
        else {
            to->key = key0;
            to->value = from0->value; // note that this may be the same address
            from0++; // nach hinten schieben?
            key0 = from0->key;
        }
        to++;
    }
    *f0 = from0;
    *f1 = from1;
}


// merge sz element from the three sentinel terminated input
// sequences *f0, *f1 and *f2 to "to"
// advance *f0, *f1 and *f2 accordingly.
// require: at least sz nonsentinel elements available in f0, f1 and f2
// require: to may overwrite one of the sources as long as
//   *fx + sz is before the end of fx
template <class Key, class Value>
void merge3(KNElement<Key, Value>** f0,
    KNElement<Key, Value>** f1,
    KNElement<Key, Value>** f2,
    KNElement<Key, Value>* to, int sz)
{
    KNElement<Key, Value>* from0 = *f0;
    KNElement<Key, Value>* from1 = *f1;
    KNElement<Key, Value>* from2 = *f2;
    KNElement<Key, Value>* done = to + sz;
    Key      key0 = from0->key;
    Key      key1 = from1->key;
    Key      key2 = from2->key;

    if (key0 < key1) {
        if (key1 < key2) { goto s012; }
        else {
            if (key2 < key0) { goto s201; }
            else { goto s021; }
        }
    }
    else {
        if (key1 < key2) {
            if (key0 < key2) { goto s102; }
            else { goto s120; }
        }
        else { goto s210; }
    }

#define Merge3Case(a,b,c)\
  s ## a ## b ## c :\
  if (to == done) goto finish;\
  to->key = key ## a;\
  to->value = from ## a -> value;\
  to++;\
  from ## a ++;\
  key ## a = from ## a -> key;\
  if (key ## a < key ## b) goto s ## a ## b ## c;\
  if (key ## a < key ## c) goto s ## b ## a ## c;\
  goto s ## b ## c ## a;

    // the order is choosen in such a way that 
    // four of the trailing gotos can be eliminated by the optimizer
    Merge3Case(0, 1, 2);
    Merge3Case(1, 2, 0);
    Merge3Case(2, 0, 1);
    Merge3Case(1, 0, 2);
    Merge3Case(0, 2, 1);
    Merge3Case(2, 1, 0);

finish:
    *f0 = from0;
    *f1 = from1;
    *f2 = from2;
}


// merge sz element from the three sentinel terminated input
// sequences *f0, *f1, *f2 and *f3 to "to"
// advance *f0, *f1, *f2 and *f3 accordingly.
// require: at least sz nonsentinel elements available in f0, f1, f2 and f2
// require: to may overwrite one of the sources as long as
//   *fx + sz is before the end of fx
template <class Key, class Value>
void merge4(KNElement<Key, Value>** f0,
    KNElement<Key, Value>** f1,
    KNElement<Key, Value>** f2,
    KNElement<Key, Value>** f3,
    KNElement<Key, Value>* to, int sz)
{
    KNElement<Key, Value>* from0 = *f0;
    KNElement<Key, Value>* from1 = *f1;
    KNElement<Key, Value>* from2 = *f2;
    KNElement<Key, Value>* from3 = *f3;
    KNElement<Key, Value>* done = to + sz;
    Key      key0 = from0->key;
    Key      key1 = from1->key;
    Key      key2 = from2->key;
    Key      key3 = from3->key;

#define StartMerge4(a, b, c, d)\
  if (key##a <= key##b && key##b <= key##c && key##c <= key##d)\
    goto s ## a ## b ## c ## d;

    StartMerge4(0, 1, 2, 3);
    StartMerge4(1, 2, 3, 0);
    StartMerge4(2, 3, 0, 1);
    StartMerge4(3, 0, 1, 2);

    StartMerge4(0, 3, 1, 2);
    StartMerge4(3, 1, 2, 0);
    StartMerge4(1, 2, 0, 3);
    StartMerge4(2, 0, 3, 1);

    StartMerge4(0, 2, 3, 1);
    StartMerge4(2, 3, 1, 0);
    StartMerge4(3, 1, 0, 2);
    StartMerge4(1, 0, 2, 3);

    StartMerge4(2, 0, 1, 3);
    StartMerge4(0, 1, 3, 2);
    StartMerge4(1, 3, 2, 0);
    StartMerge4(3, 2, 0, 1);

    StartMerge4(3, 0, 2, 1);
    StartMerge4(0, 2, 1, 3);
    StartMerge4(2, 1, 3, 0);
    StartMerge4(1, 3, 0, 2);

    StartMerge4(1, 0, 3, 2);
    StartMerge4(0, 3, 2, 1);
    StartMerge4(3, 2, 1, 0);
    StartMerge4(2, 1, 0, 3);

#define Merge4Case(a, b, c, d)\
  s ## a ## b ## c ## d:\
  if (to == done) goto finish;\
  to->key = key ## a;\
  to->value = from ## a -> value;\
  to++;\
  from ## a ++;\
  key ## a = from ## a -> key;\
  if (key ## a < key ## c) {\
    if (key ## a < key ## b) { goto s ## a ## b ## c ## d; }\
    else                     { goto s ## b ## a ## c ## d; }\
  } else {\
    if (key ## a < key ## d) { goto s ## b ## c ## a ## d; }\
    else                     { goto s ## b ## c ## d ## a; }\
  }    

    Merge4Case(0, 1, 2, 3);
    Merge4Case(1, 2, 3, 0);
    Merge4Case(2, 3, 0, 1);
    Merge4Case(3, 0, 1, 2);

    Merge4Case(0, 3, 1, 2);
    Merge4Case(3, 1, 2, 0);
    Merge4Case(1, 2, 0, 3);
    Merge4Case(2, 0, 3, 1);

    Merge4Case(0, 2, 3, 1);
    Merge4Case(2, 3, 1, 0);
    Merge4Case(3, 1, 0, 2);
    Merge4Case(1, 0, 2, 3);

    Merge4Case(2, 0, 1, 3);
    Merge4Case(0, 1, 3, 2);
    Merge4Case(1, 3, 2, 0);
    Merge4Case(3, 2, 0, 1);

    Merge4Case(3, 0, 2, 1);
    Merge4Case(0, 2, 1, 3);
    Merge4Case(2, 1, 3, 0);
    Merge4Case(1, 3, 0, 2);

    Merge4Case(1, 0, 3, 2);
    Merge4Case(0, 3, 2, 1);
    Merge4Case(3, 2, 1, 0);
    Merge4Case(2, 1, 0, 3);

finish:
    *f0 = from0;
    *f1 = from1;
    *f2 = from2;
    *f3 = from3;
}

//  C++ done

// C bindings for C++

#include <limits.h>

void* KNHeap__create(void) {
    return new KNHeap<wl_syscalls__fd_allocator__size_t, void*>(wl_syscalls__fd_allocator__size_t_MAX, -wl_syscalls__fd_allocator__size_t_MAX);
}

void KNHeap__destroy(void* instance) {
    delete reinterpret_cast<KNHeap<wl_syscalls__fd_allocator__size_t, void*>*>(instance);
}

int   KNHeap__getSize(void* instance) {
    return reinterpret_cast<KNHeap<wl_syscalls__fd_allocator__size_t, void*>*>(instance)->getSize();
}
void  KNHeap__getMin(void* instance, wl_syscalls__fd_allocator__size_t* key, void** value) {
    reinterpret_cast<KNHeap<wl_syscalls__fd_allocator__size_t, void*>*>(instance)->getMin(key, value);
}
void  KNHeap__deleteMin(void* instance, wl_syscalls__fd_allocator__size_t* key, void** value) {
    reinterpret_cast<KNHeap<wl_syscalls__fd_allocator__size_t, void*>*>(instance)->deleteMin(key, value);
}
void  KNHeap__insert(void* instance, wl_syscalls__fd_allocator__size_t key, void* value) {
    reinterpret_cast<KNHeap<wl_syscalls__fd_allocator__size_t, void*>*>(instance)->insert(key, value);
}

void* ShrinkingVectorIndexAllocator__create(void) {
    return new ShrinkingVectorIndexAllocator();
}
void   ShrinkingVectorIndexAllocator__destroy(void* instance) {
    delete reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance);
}
size_t ShrinkingVectorIndexAllocator__size(void* instance) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->size();
}
size_t ShrinkingVectorIndexAllocator__capacity(void* instance) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->capacity();
}
size_t ShrinkingVectorIndexAllocator__add(void* instance, void* value, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->add(value, callback);
}
size_t ShrinkingVectorIndexAllocator__reuse(void* instance, size_t index, void* value, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->reuse(index, value, callback);
}
bool ShrinkingVectorIndexAllocator__index_is_valid(void* instance, size_t index) {
    int a;
    size_t b;
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->index_if_valid(index, &a, &b) != NULL;
}
void* ShrinkingVectorIndexAllocator__data(void* instance, size_t index) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->operator[](index);
}
bool   ShrinkingVectorIndexAllocator__remove(void* instance, int index) {
    return reinterpret_cast<ShrinkingVectorIndexAllocator*>(instance)->remove(index);
}

// C bindings done, C++ no longer needed

wl_syscalls__fd_allocator* wl_syscalls__fd_allocator__create(void) {
    wl_syscalls__fd_allocator* fd = (wl_syscalls__fd_allocator*)malloc(sizeof(wl_syscalls__fd_allocator));
    if (fd == NULL) {
        return NULL;
    }
    fd->used = ShrinkingVectorIndexAllocator__create();
    fd->recycled = KNHeap__create();
    return fd;
}

void wl_syscalls__fd_allocator__destroy(wl_syscalls__fd_allocator* fd) {
    ShrinkingVectorIndexAllocator__destroy(fd->used);
    KNHeap__destroy(fd->recycled);
    free(fd);
}

size_t wl_syscalls__fd_allocator__size(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator) {
    return ShrinkingVectorIndexAllocator__size(wl_syscalls__fd_allocator->used);
}

size_t wl_syscalls__fd_allocator__capacity(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator) {
    return ShrinkingVectorIndexAllocator__capacity(wl_syscalls__fd_allocator->used);
}

int wl_syscalls__fd_allocator__allocate_fd(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, void* data, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback) {
    if (KNHeap__getSize(wl_syscalls__fd_allocator->recycled) > 0) {
        void* null_data;
        int fd;
        KNHeap__deleteMin(wl_syscalls__fd_allocator->recycled, &fd, &null_data);
        return ShrinkingVectorIndexAllocator__reuse(wl_syscalls__fd_allocator->used, fd, data, callback);
    }
    else {
        return ShrinkingVectorIndexAllocator__add(wl_syscalls__fd_allocator->used, data, callback);
    }
}

bool wl_syscalls__fd_allocator__fd_is_valid(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd) {
    return ShrinkingVectorIndexAllocator__index_is_valid(wl_syscalls__fd_allocator->used, fd);
}

void* wl_syscalls__fd_allocator__get_value_from_fd(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd) {
    return ShrinkingVectorIndexAllocator__data(wl_syscalls__fd_allocator->used, fd);
}

void wl_syscalls__fd_allocator__deallocate_fd(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd) {
    size_t diff = ShrinkingVectorIndexAllocator__capacity(wl_syscalls__fd_allocator->used);
    if (!ShrinkingVectorIndexAllocator__remove(wl_syscalls__fd_allocator->used, fd)) {
        if (wl_miniobj_debug) printf("ATTEMPTING TO DEALLOCATE INVALID INDEX: %d\n", fd);
        return;
    }
    size_t cap = ShrinkingVectorIndexAllocator__capacity(wl_syscalls__fd_allocator->used);
    if ((diff - cap) != 0) {
        if (cap == 0) {
            KNHeap__destroy(wl_syscalls__fd_allocator->recycled);
            wl_syscalls__fd_allocator->recycled = KNHeap__create();
        }
        else if (KNHeap__getSize(wl_syscalls__fd_allocator->recycled) > 0) {
            // we need to remove size
            int size = ShrinkingVectorIndexAllocator__size(wl_syscalls__fd_allocator->used);
            void* tmp = KNHeap__create();
            int removed = -1;
            void* null_data;
            // extract valid items
            while (true) {
                if (KNHeap__getSize(wl_syscalls__fd_allocator->recycled) == 0) {
                    break;
                }
                KNHeap__deleteMin(wl_syscalls__fd_allocator->recycled, &removed, &null_data);
                if (removed >= size) {
                    break;
                }
                KNHeap__insert(tmp, removed, NULL);
            }
            // rebuild queue
            if (wl_miniobj_debug) printf("REBUILD FD RECYCLE QUEUE\n");
            KNHeap__destroy(wl_syscalls__fd_allocator->recycled);
            wl_syscalls__fd_allocator->recycled = KNHeap__create();
            removed = -1;
            while (KNHeap__getSize(tmp) != 0) {
                KNHeap__deleteMin(tmp, &removed, &null_data);
                KNHeap__insert(wl_syscalls__fd_allocator->recycled, removed, NULL);
            }
            KNHeap__destroy(tmp);
        }
    }
    else {
        KNHeap__insert(wl_syscalls__fd_allocator->recycled, fd, NULL);
    }
}
