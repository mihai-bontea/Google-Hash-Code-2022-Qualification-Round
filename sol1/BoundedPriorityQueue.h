#pragma once

#include <queue>
#include <vector>
#include <functional>

template <typename T, typename Compare = std::less<T>>
class BoundedPriorityQueue{
public:
    explicit BoundedPriorityQueue(size_t max_size)
    : max_size(max_size)
    {}

    void push(const T& value)
    {
        // There is room left
        if (pq.size() < max_size)
            pq.push(value);
        // Capacity reached, but new value is better than worst value in queue
        else if (comp(value, pq.top()))
        {
            pq.pop();
            pq.push(value);
        }
        // else, discard new value
    }

    std::vector<T> extract_sorted()
    {
        std::vector<T> result;
        while (!pq.empty())
        {
            result.push_back(pq.top());
            pq.pop();
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    [[nodiscard]] bool empty() const
    {
        return pq.empty();
    }

private:
    size_t max_size;
    Compare comp;
    std::priority_queue<T, std::vector<T>, Compare> pq;
};
