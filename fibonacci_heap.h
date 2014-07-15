/* Copyright (c) 2014 Weidong Fang
 *
 * https://github.com/fangwd/graph
 *
 * Distributed under MIT license
 */

#ifndef _FIBONACCI_HEAP__H_
#define _FIBONACCI_HEAP__H_

#include <cassert>
#include <cstddef>

#include <vector>
#include <fstream>
#include <ostream>

namespace fibonacci {

static const size_t MAX_DEGREE = 64;

template<class MyNode>
class HeapNode {
private:
    MyNode  *next_;
    MyNode  *prev_;
    MyNode  *parent_;
    MyNode  *child_;
    size_t     degree_;     // number of children
    double     priority_;
    bool       marked_;

    template<class NodeType> friend class Heap;

    void init(double priority) {
        next_ = prev_ = (MyNode *)this;
        parent_ = child_ = NULL;
        degree_ = 0;
        marked_ = false;
        priority_ = priority;
    }

public:
    HeapNode(double priority) {
        init(priority);
    }

    virtual ~HeapNode() {
    }

    double priority() {
        return priority_;
    }

    virtual const char *name() { return ""; }
};

template<class MyNode>
class Heap {
private:
    MyNode *min_root_;
    MyNode *root_map_[MAX_DEGREE];

public:
    Heap() : min_root_(NULL) {
    }

    ~Heap() {
        clear(true);
    }

    void clear(bool free_nodes = false) {
        if (min_root_) {
            if (free_nodes) {
                MyNode *node = min_root_;
                do {
                    MyNode *next = node->next_;
                    delete_tree(node);
                    node = next;
                } while (node != min_root_);
            }
            min_root_ = NULL;
        }
    }

    bool empty() const {
        return min_root_ == NULL;
    }

    void insert(MyNode *node, double priority) {
        // Node may be reinserted
        node->init(priority);
        if (!min_root_) {
            min_root_ = node;
        }
        else {
            insert_after(min_root_, node);
            if (node->priority_ < min_root_->priority_) {
                min_root_ = node;
            }
        }
    }

    MyNode* get_min() {
        return min_root_;
    }

    MyNode* pop_min() {
        if (!min_root_) return NULL;

        MyNode *min = min_root_;

        if (min->next_ == min) {
            min_root_ = NULL;
        }
        else {
            min_root_ = min->next_;
            unlink_node(min);
        }

        if (min->child_) {
            MyNode *node = min->child_;

            do {
                node->parent_ = NULL;
                node = node->next_;
            } while (node != min->child_);

            if (min_root_ == NULL) {
                min_root_ = min->child_;
            }
            else {
                splice(min_root_, min->child_);
            }

            min->child_ = NULL;
        }

        if (min_root_) {
            consolidate();
        }

        return min;
    }

    void decrease_priority(MyNode *node, double priority) {
        assert(priority < node->priority_);

        node->priority_ = priority;

        if (node->parent_ && priority < node->parent_->priority_) {
            cut(node);
        }

        if (priority < min_root_->priority_ && min_root_ != node) {
            min_root_ = node;
        }
    }

private:
    void delete_tree(MyNode *root) {
        if (root->child_) {
            MyNode *node = root->child_;
            do {
                MyNode *next = node->next_;
                delete_tree(node);
                node = next;
            } while (node != root->child_);
        }
        delete root;
    }

    bool sanity_check(MyNode *any, size_t d = 0) {
        const static int MAX_NODE = 1000;
        const static int MAX_DEPTH = 10;

        if (!any) return true;

        assert(d < MAX_DEPTH);

        MyNode *n = any;
        size_t i = 0;

        do {
            i++;
            n = n->next_;
        } while (n != any && i < MAX_NODE);

        assert(i < MAX_NODE);

        n = any;
        i = 0;

        do {
            i++;
            n = n->prev_;
        } while (n != any && i < MAX_NODE);

        assert(i < MAX_NODE);

        n = any;
        do {
            if (n->child_) {
                assert(n->child_ != n);
                sanity_check(n->child_, d + 1);
            }
            n = n->next_;
        } while (n != any);

        return n == any;
    }

    /**
     * Splices two lists represented by head and tail respectively. After the
     * splicing, head and tail will be linked together using their prev/next
     * links; head's tail and tail's head become the tail and head of the
     * resulting list respectively.
     *
     * Note that for a circular list, we can view any node as the head (or the
     * tail) and head->next points to the tail and tail->prev points to the
     * head.
     */
    void splice(MyNode* head, MyNode* tail) {
        head->next_->prev_ = tail->prev_;
        tail->prev_->next_ = head->next_;
        head->next_ = tail;
        tail->prev_ = head;
    }

    /**
     * Inserts newNode after node
     */
    void insert_after(MyNode* node, MyNode* node_after) {
        node_after->next_ = node->next_;
        node_after->prev_ = node;
        node->next_->prev_ = node_after;
        node->next_ = node_after;
    }

    void push_child(MyNode *parent, MyNode *child) {
        if (!parent->child_) {
            child->next_ = child->prev_ = child;
            parent->child_ = child;
        }
        else {
            insert_after(parent->child_, child);
        }
        child->parent_ = parent;
        parent->degree_++;
    }

    void unlink_node(MyNode* node) {
        assert(node->next_ != node);
        node->next_->prev_ = node->prev_;
        node->prev_->next_ = node->next_;
    }

    void root_remove(MyNode *root) {
        assert(root_map_[root->degree_] == root);

        if (root == min_root_) {
            if (min_root_ == min_root_->next_) {
                min_root_ = NULL;
            }
            else {
                min_root_ = min_root_->next_;
                unlink_node(root);
            }
        }
        else {
            unlink_node(root);
        }

        root_map_[root->degree_] = NULL;
    }

    void root_push(MyNode *root) {
        assert(root_map_[root->degree_] == NULL);

        if (!min_root_) {
            root->next_ = root->prev_ = root;
            min_root_ = root;
        }
        else {
            insert_after(min_root_, root);
        }

        root_map_[root->degree_] = root;

        if (root->priority_ < min_root_->priority_) {
            min_root_ = root;
        }
    }

    void consolidate() {
        MyNode *min = min_root_;

        min_root_ = NULL;

        for (size_t i = 0; i < MAX_DEGREE; i++) {
            root_map_[i] = NULL;
        }

        do {
            MyNode *next = min->next_ == min ? NULL : min->next_;

            if (min->next_ != min) {
                unlink_node(min);
            }

            for (;;) {
                min->marked_ = false;

                MyNode *root = root_map_[min->degree_];
                if (root == NULL) {
                    root_push(min);
                    break;
                }

                root_remove(root);

                if (root->priority_ < min->priority_) {
                    push_child(root, min);
                    min = root;
                }
                else {
                    push_child(min, root);
                    root->marked_ = false;
                }
            }

            min = next;

        } while (min != NULL);
    }

    void cut(MyNode* node) {
        for (;;) {
            MyNode *parent = node->parent_;

            // Remove child from the child list of parent
            if (parent->child_ == node) {
                if (node->next_ != node) {
                    parent->child_ = node->next_;
                    unlink_node(node);
                } else {
                    parent->child_ = NULL;
                }
            }
            else {
                assert(node->next_ != node);
                unlink_node(node);
            }

            node->parent_ = NULL;

            // Decrement parent's degree
            --parent->degree_;



            // Add child to the root list
            insert_after(min_root_, node);

            // Unmark child
            node->marked_ = false;

            // Cascading cut
            if (!parent->parent_) {
                break;
            }

            if (!parent->marked_) {
                parent->marked_ = true;
                break;
            }
            else {
                node = parent;
            }
        }
    }

    typedef std::vector<std::vector<const char*>* > NameArray;

public:
    bool save(const char *filename) {
        std::ofstream ofs(filename);

        ofs << "digraph G {\n";
        ofs << "ranksep=.5; size = \"10,5\";\n";
        ofs << "node [shape=box,width=0.8,height=0.3];\n";

        if (min_root_) {
            NameArray name_array;

            push_names(min_root_, name_array, 0);

            for (size_t i = 0; i < name_array.size(); i++) {
                std::vector<const char*>& ar = *name_array[i];
                ofs << "{ rank=same;\n";
                for (size_t j = 0; j < ar.size(); j++) {
                    if (i == 0 && j == 0) {
                        ofs << ar[j] << " [style=filled, fillcolor=red]; ";
                    }
                    else {
                        ofs << ar[j] << "; ";
                    }
                }
                ofs << "}\n";
            }

            MyNode *node = min_root_;

            do {
                write_siblings(ofs, node, node->next_);
                write_tree(ofs, node);
                node = node->next_;
            } while (node != min_root_);

            for (size_t i = 0; i < name_array.size(); i++) {
                delete name_array[i];
            }
        }

        ofs << "}\n";

        return true;
    }
private:
    void write_tree(std::ostream& os, MyNode *root) {
        if (root->child_) {
            MyNode *child = root->child_;
            do {
                write_siblings(os, child, child->next_);
                write_parent_child(os, root, child);
                write_tree(os, child);
                child = child->next_;
            } while (child != root->child_);
        }
    }

    void write_siblings(std::ostream& os, MyNode *prev, MyNode *next) {
        os << '\t' << prev->name() << "->" << next->name() << ";\n";
        os << '\t' << next->name() << "->" << prev->name() << " [weight=0.1,style=dashed];\n";
    }

    void write_parent_child(std::ostream& os, MyNode *parent, MyNode *child) {
        os << '\t' << parent->name() << "->" << child->name() << " [color=blue];\n";
        os << '\t' << child->name() << "->" << parent->name() << " [color=blue, style=dashed];\n";
    }

    void push_names(MyNode *first_node, NameArray& name_array, size_t level) {
        if (name_array.size() <= level) {
            name_array.resize(level + 1);
            name_array[level] = new std::vector<const char *>();
        }

        std::vector<const char *>& ar = *name_array[level];

        MyNode *node = first_node;
        do {
            ar.push_back(node->name());
            if (node->child_) {
                push_names(node->child_, name_array, level + 1);
            }
            node = node->next_;
        } while (node != first_node);

    }
};

} // namespace fibonacci

#endif // _FIBONACCI_HEAP__H_
