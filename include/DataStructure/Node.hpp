#pragma once

namespace Ds
{
    template <typename T>
    class CircularLinkedList;

    template <typename T>
    class Node
    {
        friend class CircularLinkedList<T>;

    public:
        bool operator==(const Node &rhs) const
        {
            return value == rhs.value;
        }

        T value;

    private:
        Node *next;
        Node *prev;
    };
}