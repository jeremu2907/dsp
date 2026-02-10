#pragma once

#include "Node.hpp"

namespace Ds
{
    template <typename T>
    class CircularLinkedList
    {
    public:
        size_t emplace(const T& value)
        {
            Node<T> *newNode = new Node<T>();
            newNode->value = value;
            if (m_size == 0)
            {
                newNode->next = newNode;
                newNode->prev = newNode;
                m_begin = newNode;
                m_end = newNode;
                m_current = newNode;
            }
            else
            {
                Node<T> *endNode = m_end;
                Node<T> *beginNode = m_begin;
                endNode->next = newNode;
                newNode->prev = endNode;
                newNode->next = beginNode;
                beginNode->prev = newNode;
                m_end = newNode;
            }
            ++m_size;
            return m_size;
        }

        size_t erase(const T value)
        {
            if (m_size == 0)
            {
                return 0;
            }
            Node<T> *node = m_begin;
            do
            {
                if (node->value == value)
                {
                    if (m_size == 1)
                    {
                        delete node;
                        m_end = nullptr;
                        m_begin = nullptr;
                        m_current = nullptr;
                        m_size = 0;
                        return m_size;
                    }
                    node->prev->next = node->next;
                    node->next->prev = node->prev;
                    if (node == m_begin)
                    {
                        m_begin = node->next;
                    }

                    if (node == m_end)
                    {
                        m_end = node->prev;
                    }
                    if (node == m_current)
                    {
                        m_current = node->next;
                    }

                    delete node;
                    --m_size;
                    break;
                }
                node = node->next;
            } while (node != m_begin);
            return m_size;
        }

        size_t size() const
        {
            return m_size;
        }

        Node<T> *reset()
        {
            m_current = m_begin;
            return m_current;
        }

        Node<T> *next()
        {
            m_current = m_current->next;
            return m_current;
        }

        Node<T> *prev()
        {
            m_current = m_current->prev;
            return m_current;
        }

        Node<T> *current()
        {
            return m_current;
        }

        bool empty()
        {
            return m_size == 0;
        }

    private:
        size_t m_size = 0;
        Node<T> *m_end = nullptr;
        Node<T> *m_begin = nullptr;
        Node<T> *m_current = nullptr;
    };
}