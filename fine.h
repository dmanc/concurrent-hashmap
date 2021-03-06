#ifndef FINE_H
#define FINE_H

#include <atomic>
#include "hashtable.h"

#define START_NUM_BUCKETS_FINE 16
#define RESIZE_FACTOR_FINE 0.75

class Node_Fine {
    public:
        uint32_t key, value;
        Node_Fine* next;
        Node_Fine(uint32_t K, uint32_t V): key(K), value(V), next(NULL){}
};

class Bucket_Fine {
    public:
        uint32_t size;
        Node_Fine *head;
        Bucket_Fine(): size(0), head(NULL) {}
        ~Bucket_Fine() {
            Node_Fine* head = this->head;
            while(head != NULL) {
                Node_Fine* next = head->next;
                //delete head;
                head = next;
            }
        }

        bool hasKey(uint32_t k) {
            Node_Fine* head = this->head;
            while(head != NULL) {
                if(head->key == k) {
                    return true;
                }

                head = head->next;
            }
            //Didn't find key
            return false;

        }

        void add(uint32_t k, uint32_t v) {
            if(head == NULL) {
                head = new Node_Fine(k, v);
            } else {
                Node_Fine* node = new Node_Fine(k, v);
                node->next = head;
                head = node;
            }
            size++;
        }

        uint32_t get(uint32_t k) {
            Node_Fine* head = this->head;
            while(head != NULL) {
                if(head->key == k) {
                    return head->value;
                }

                head = head->next;
            }
            //Didn't find key
            return 0xdeadbeef;
        }
};

class Fine : public HashTable {
    private:
        uint32_t num_buckets;
        Bucket_Fine* buckets;
        mutex* locks;
        mutex resize_lock;
        atomic<uint32_t> entries_at;
        bool toresize;

        //protected field entries

        double balanceFactor() {
            return (double) entries_at / (double) num_buckets;
        }

        Bucket_Fine* getBucketForKey(uint32_t key) {
            return buckets + (hash_(key) % num_buckets);
        }

        mutex* getLockForKey(uint32_t key) {
            return locks + (hash_(key) % START_NUM_BUCKETS_FINE);
        }

        void resize() {
            //TODO
            //Save old buckets
            Bucket_Fine* old_buckets = buckets;
            uint32_t old_size = num_buckets;

            //Make new buckets with old size
            num_buckets *= 2;
            buckets = new Bucket_Fine[num_buckets];
            entries_at = 0;

            //Insert all old elements into new table
            for(uint32_t i = 0; i < old_size; i++) {
                Bucket_Fine* b = &old_buckets[i];
                Node_Fine* head = b->head;
                while(head != NULL) {
                    put_free(head->key, head->value);
                    head = head->next;
                }
            }
            
            //delete[] old_buckets;
        }
    public:
        Fine(bool toresize = true) : num_buckets(START_NUM_BUCKETS_FINE) {
            buckets = new Bucket_Fine[START_NUM_BUCKETS_FINE];
            locks = new mutex[START_NUM_BUCKETS_FINE];
            entries_at = 0;
            this->toresize = toresize;
        }
        uint32_t get(uint32_t key) {
            mutex* bucket_lock = getLockForKey(key);
            lock_guard<mutex> lock_g(*bucket_lock);
            Bucket_Fine* b = getBucketForKey(key);
            return b->get(key);
        }

        void put(uint32_t key, uint32_t val) {
            mutex* bucket_lock = getLockForKey(key);
            bucket_lock->lock();
            Bucket_Fine* b = getBucketForKey(key);
            b->add(key, val);
            entries_at++;
            bucket_lock->unlock();


            if(toresize) {
                //Only one thread should resize
                if(balanceFactor() >= RESIZE_FACTOR_FINE && resize_lock.try_lock()) {
                    //Acquire all locks
                    for(int i = 0; i < START_NUM_BUCKETS_FINE; i++) {
                        locks[i].lock();
                    }
                    resize();
                    //Free all locks
                    for(int i = 0; i < START_NUM_BUCKETS_FINE; i++) {
                        locks[i].unlock();
                    }
                    resize_lock.unlock();
                }
            }
            return;
        }

        //Puts without a lock, helper for resizing
        void put_free(uint32_t key, uint32_t val) {
            Bucket_Fine* b = getBucketForKey(key);
            b->add(key, val);
            entries_at++;
            return;
        }


        bool hasKey(uint32_t key) {

            mutex* bucket_lock = getLockForKey(key);
            lock_guard<mutex> lock_g(*bucket_lock);
            Bucket_Fine* b = getBucketForKey(key);

            //return false if bucket empty
            if(b->size == 0) {
                return false;
            }

            return b->hasKey(key);
        }

        uint32_t size() {
            return entries_at;
        }

        bool isEmpty() {
            return Fine::size() == 0;
        }


};

#endif
