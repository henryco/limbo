//
// Created by henryco on 07/01/25.
//

#ifndef UNDO_HISTORY_H
#define UNDO_HISTORY_H

#include <functional>

namespace limbo::undo {

    template<typename T>
    struct record {
        using rollback_fn = std::function<void(T *)>;
        using cleanup__fn = std::function<void()>;

        const std::size_t  id;
        const char        *name = nullptr;
        const rollback_fn *undo = nullptr;
        const rollback_fn *redo = nullptr;
        const cleanup__fn *dstr = nullptr;
        record            *prev = nullptr;
        record            *next = nullptr;

        ~record() {
            if (undo != nullptr)
                delete undo;
            if (redo != nullptr)
                delete redo;
            if (next != nullptr)
                delete next;
            if (dstr != nullptr) {
                dstr->operator()();
                delete dstr;
            }
        }
    };

    template<typename T>
    struct history {
        static constexpr int cap_history = 64;

        record<T> *records = nullptr;
        record<T> *current = nullptr;
        record<T> *target_ = nullptr;
        record<T> *root___ = nullptr;

        int capacity = cap_history;
        int size     = 0;

        const char *init_str = nullptr;

        history() = default;

        explicit history(const char *initial) : init_str(initial) {
            push(initial, nullptr, nullptr);
        }

        ~history() {
            record<T> *head = records;
            while (head != nullptr) {
                record<T> *prev = head->prev;
                head->next      = nullptr;
                delete head;
                head = prev;
            }
        }

        bool can_undo() {
            return target_ != nullptr && target_->prev != nullptr;
        }

        bool can_redo() {
            return target_ != nullptr && target_->next != nullptr;
        }

        void push(const char *name,
            std::function<void(T *)> *undo,
            std::function<void(T *)> *redo,
            std::function<void()> *cleanup = nullptr)
        {
            static std::size_t counter = 0;

            if (records == nullptr || current == nullptr) {
                records = new record<T>(counter++, name, undo, redo, cleanup, nullptr, nullptr);
                current = records;
                target_ = records;
                root___ = records;
                size    = 1;
                return;
            }

            if (records->id == current->id && current == records) {
                records->next = new record<T>(counter++, name, undo, redo, cleanup, records, nullptr);
                records       = records->next;
                current       = records;
                target_       = records;
                size++;

                while ((size - capacity) > 0) {
                    auto next___ = root___->next;

                    next___->prev = nullptr;
                    root___->prev = nullptr;
                    root___->next = nullptr;
                    delete root___;

                    root___ = next___;
                    size--;
                }

                return;
            }

            // - - - - - - - - - - - // records
            //           x           // current
            //           > > > > x   // target_

            record<T> *head = records;
            while (head != nullptr) {
                if (head->id == current->id && current == head) {
                    current->next = new record<T>(counter++, name, undo, redo, cleanup, current, nullptr);
                    current       = current->next;
                    records       = current;
                    target_       = current;
                    size++;
                    return;
                }
                record<T> *prev = head->prev;
                head->next      = nullptr;
                delete head;
                head = prev;
                size--;
            }
        }

        bool move(const std::size_t id) {
            record<T> *head = records;
            while (head != nullptr) {
                if (head->id == id) {
                    target_ = head;
                    return true;
                }
                head = head->prev;
            }
            return false;
        }

        void redo() {
            if (records == nullptr || current == nullptr || target_ == nullptr)
                return;
            if (target_->next != nullptr)
                target_ = target_->next;
        }

        void undo() {
            if (records == nullptr || current == nullptr || target_ == nullptr)
                return;
            if (target_->prev != nullptr)
                target_ = target_->prev;
        }

        void apply(T *state) {
            if (records == nullptr || current == nullptr || target_ == nullptr)
                return;

            if (target_->id == current->id && current == target_)
                return;

            const bool direction = target_->id >= current->id;
            while (current != nullptr) {
                if (current->id == target_->id && current == target_)
                    return;

                if (!direction && current->undo != nullptr)
                    current->undo->operator()(state);

                current = direction ? current->next : current->prev;

                if (direction && current != nullptr && current->redo != nullptr)
                    current->redo->operator()(state);
            }
            target_ = current;
        }

        void reset() {
            record<T> *head = records;
            while (head != nullptr) {
                record<T> *prev = head->prev;
                head->next      = nullptr;
                delete head;
                head = prev;
            }

            records = nullptr;
            current = nullptr;
            target_ = nullptr;
            root___ = nullptr;

            capacity = cap_history;
            size     = 0;

            if (init_str != nullptr)
                push(init_str, nullptr, nullptr);
        }
    };

}

#endif //UNDO_HISTORY_H
