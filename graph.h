/* Copyright (c) 2014 Weidong Fang
 *
 * https://github.com/fangwd/graph
 *
 * Distributed under MIT license
 */
#ifndef _GRAPH__H_
#define _GRAPH__H_

#include <cassert>
#include <utility>
#include <vector>
#include <ostream>

#include "fibonacci_heap.h"

#define INFINITY    2147483647

class Vertex;

struct Arc {
    Vertex *tail;
    Vertex *head;
    double  weight;
    void   *data;   // Related user data, e.g. ForeignKey*
    Arc    *next;

    Arc(Vertex *tail, Vertex *head, double weight, void *data = NULL) {
        this->tail = tail;
        this->head = head;
        this->weight = weight;
        this->data = data;
        next = NULL;
    }
};

/**
 * HeapNode base is for Dijkstra to keep track of the path weight from source to
 * any node in the graph
 */
struct Vertex : public fibonacci::HeapNode<Vertex> {
    size_t  id;
    Arc    *first_arc;
    Arc    *path_arc;       // runtime use: arc leading to this node in a path
    bool    usable_;       // KSP non-loop control

    Vertex(size_t id) : fibonacci::HeapNode<Vertex>(INFINITY) {
        this->id     = id;
        this->first_arc = NULL;
        this->path_arc   = NULL;
        usable_ = true;
    }

    virtual ~Vertex() {
        Arc *arc = first_arc;
        while (arc) {
            Arc *next = arc->next;
            delete arc;
            arc = next;
        }
    }

    void set_weight(Vertex *node, double weight, void *data=NULL) {
        Arc *arc = new Arc(this, node, weight, data);
        arc->next = first_arc;
        first_arc = arc;
    }

    friend std::ostream& operator<<(std::ostream& os, const Vertex& v);
};

class Path;

std::ostream& operator<<(std::ostream& os, const Path& p);

/**
 * HeapNode base to keep track of all candidate paths from source to
 * destination in Yen's KSP.
 */
class Path : public fibonacci::HeapNode<Path> {
public:
    struct Node {
        Arc     *arc;
        double   weight;  // weight to get to the end of the arc
        Node    *next;

        Node(Arc *arc) {
            this->arc  = arc;
            this->weight = arc->head->priority();
            this->next = NULL;
        }

        Node(const Node *other) {
            arc  = other->arc;
            weight = other->weight;
            next = NULL;
        }
    };
private:
    Node *first_node_;
    Node *last_node_;

    friend class Graph;
    friend std::ostream& operator<<(std::ostream& os, const Path& p);

public:
    Path(double weight=0.0) : fibonacci::HeapNode<Path>(weight) {
        first_node_ = NULL;
        last_node_ = NULL;
    }

    virtual ~Path() {
        Node *m = first_node_;
        while (m) {
            Node *n = m->next;
            delete m;
            m = n;
        }
    }

    /**
     * Called when we have just finished the Dijkstra algorithm, where every
     * vertex has an associated priority.
     */
    Path(Vertex *v) : fibonacci::HeapNode<Path>(v->priority()) {
        first_node_ = NULL;
        for (; v->path_arc; v = v->path_arc->tail) {
            Node *node = new Node(v->path_arc);
            push_front(node);
        }
    }

    void enable_nodes(bool val) {
        Path::Node *node = first_node_;
        while (node) {
            Arc *arc = node->arc;
            arc->tail->usable_ = val;
            if (node != last_node_) {
                arc->head->usable_ = val;
            }
            node = node->next;
        }
    }

    double weight() const {
        return last_node_ ? last_node_->weight : 0.0;
    }

    void push_front(Node *node) {
        if (first_node_ == NULL) {
            first_node_ = last_node_ = node;
        }
        else {
            node->next = first_node_;
            first_node_ = node;
        }
    }

    void push_back(Node *node) {
        if (first_node_ == NULL) {
            first_node_ = last_node_ = node;
        }
        else {
            last_node_->next = node;
            last_node_ = node;
        }
    }

    Path *root_path(Node *end) {
        Path *path = new Path();
        Node *node = first_node_;
        while (node != end) {
            Node *copy = new Node(node);
            path->push_back(copy);
            node = node->next;
        }
        return path;
    }

    Node* next_node(Path *root_path) {
        Node *n1 = first_node_;
        Node *n2 = root_path->first_node_;

        while (n1 != NULL && n2 != NULL) {
            if (n1->arc != n2->arc) {
                return NULL;
            }
            n1 = n1->next;
            n2 = n2->next;
        }

        return n1;
    }

    void merge_delete(Path *other) {
        if (!first_node_) {
            first_node_ = other->first_node_;
            last_node_ = other->last_node_;
        }
        else {
            double w = this->weight();
            Node *n = other->first_node_;
            while (n) {
                n->weight += w;
                n = n->next;
            }

            last_node_->next = other->first_node_;
            last_node_ = other->last_node_;
        }
        other->first_node_ = NULL;
        other->last_node_ = NULL;
        delete other;
    }

    Node *first_node() const { return first_node_; }
};

class Graph {
private:
    std::vector<Vertex*>    node_list_;
    fibonacci::Heap<Vertex> node_heap_;

    std::vector<std::pair<Arc*, double> > removal_list_;

public:
    Graph(size_t size=0) {
        if (size > 0) set_size(size);
    }

    void set_size(size_t size) {
        for (size_t i = 0; i < node_list_.size(); i++) {
            delete node_list_[i];
        }
        node_list_.reserve(size);
        node_list_.clear();
        for (size_t i = 0; i < size; i++) {
            node_list_.push_back(new Vertex(i));
        }
    }

    void set_weight(size_t s, size_t t, double weight, void *data=NULL) {
        node_list_[s]->set_weight(node_list_[t], weight, data);
    }

    ~Graph() {
        for (size_t i = 0; i < node_list_.size(); i++) {
            delete node_list_[i];
        }
    }

    Path *shortest_path(size_t s, size_t t) {
        return shortest_path(node_list_[s], node_list_[t]);
    }

    Path *shortest_path(Vertex *s, Vertex* t) {
        node_heap_.clear();

        for (size_t i = 0; i < node_list_.size(); i++) {
            if (node_list_[i]->usable_) {
                node_heap_.insert(node_list_[i], INFINITY);
                node_list_[i]->path_arc = NULL;
            }
        }

        assert(s->usable_ && t->usable_);

        node_heap_.decrease_priority(s, 0);

        while (!node_heap_.empty()) {
            Vertex* u = node_heap_.pop_min();

            if (u == t || u->priority() == INFINITY) {
                break;
            }

            for (Arc *arc = u->first_arc; arc != NULL; arc = arc->next) {
                Vertex *v = arc->head;
                if (v->usable_) {
                    double w = u->priority() + arc->weight;
                    if (w < v->priority()) {
                        node_heap_.decrease_priority(v, w);
                        v->path_arc = arc;
                    }
                }
            }
        }

        node_heap_.clear();

        if (t->priority() == INFINITY) {
            return NULL;
        }

        return new Path(t);
    }

    typedef std::vector<Path*> PathArray;

    void remove_arc(Arc *arc) {
        removal_list_.push_back(std::pair<Arc*, double>(arc, arc->weight));
        arc->weight = INFINITY;
    }

    void restore_removals() {
        for (size_t i = 0; i < removal_list_.size(); i++) {
            Arc *arc = removal_list_[i].first;
            arc->weight = removal_list_[i].second;
        }
        removal_list_.clear();
    }

    size_t find_shortest_paths(size_t s, size_t t, size_t k, PathArray& A) {
        return find_shortest_paths(node_list_[s], node_list_[t], k, A);
    }

    size_t find_shortest_paths(Vertex *s, Vertex *t, size_t k, PathArray& A) {
        fibonacci::Heap<Path> B;

        for (Path *min_path = shortest_path(s, t); min_path != NULL; ) {
            A.push_back(min_path);

            if (A.size() >= k) break;

            Path::Node *end = min_path->first_node_;

            for (; end != NULL; end = end->next) {
                Path *root_path = min_path->root_path(end);

                root_path->enable_nodes(false);
                remove_arc(end->arc);

                for (size_t j = 0; j < A.size(); j++) {
                    Path::Node *next_node = A[j]->next_node(root_path);
                    if (next_node != NULL) {
                        remove_arc(next_node->arc);
                    }
                }

                Path *spur_path = shortest_path(end->arc->tail, t);

                root_path->enable_nodes(true);

                if (spur_path == NULL) {
                    delete root_path;
                }
                else {
                    root_path->merge_delete(spur_path);
                    B.insert(root_path, root_path->weight());
                }

                restore_removals();
            }
            min_path = B.pop_min();
        }

        return A.size();
    }
};

inline std::ostream& operator<<(std::ostream& os, const Path& path) {
    Path::Node *node = path.first_node_;
    while (node) {
        Arc *arc = node->arc;
        if (node == path.first_node_) {
            os << arc->tail->id << " -> " << arc->head->id << '('
                    << node->weight << ')';
        } else {
            os << " -> " << arc->head->id << '(' << node->weight << ')';
        }
        node = node->next;
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Vertex& v) {
    os << "Vertex " << v.id << '(' << (v.path_arc ? v.path_arc->weight : -1)
            << ')';
    return os;
}

#endif // _GRAPH__H_
