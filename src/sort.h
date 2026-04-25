#pragma once
#include "common.h"

#define SORT_DECL(T) SORT_DECL_RAW(T, T)

#define SORT_DECL_RAW(T, name) void sort_unstable_##name(T* arr, usize len);

SORT_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

#define SORT_IMPL(T, cmp_fn) SORT_IMPL_RAW(T, T, cmp_fn)

#define SORT_IMPL_RAW(T, name, cmp_fn)                                                             \
                                                                                                   \
    static void insertion_sort_##name(T* arr, usize len) {                                         \
        for (usize i = 1; i < len; ++i) {                                                          \
            T key = arr[i];                                                                        \
            usize j = i;                                                                           \
                                                                                                   \
            while (j > 0 && cmp_fn(key, arr[j - 1])) {                                             \
                arr[j] = arr[j - 1];                                                               \
                j--;                                                                               \
            }                                                                                      \
            arr[j] = key;                                                                          \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void sift_down_##name(T* arr, usize len, usize node) {                                  \
        while (true) {                                                                             \
            usize child = node * 2 + 1; /* left child */                                           \
            if (child >= len) break;                                                               \
            if (child + 1 < len && cmp_fn(arr[child], arr[child + 1])) child++;                    \
                                                                                                   \
            if (cmp_fn(arr[node], arr[child])) {                                                   \
                T tmp = arr[node];                                                                 \
                arr[node] = arr[child];                                                            \
                arr[child] = tmp;                                                                  \
                node = child;                                                                      \
            } else {                                                                               \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void heap_sort_##name(T* arr, usize len) {                                              \
        if (len <= 1) return;                                                                      \
                                                                                                   \
        for (usize i = len / 2; i-- > 0;) {                                                        \
            sift_down_##name(arr, len, i);                                                         \
        }                                                                                          \
        for (usize end = len - 1; end > 0; --end) {                                                \
            T tmp = arr[0];                                                                        \
            arr[0] = arr[end];                                                                     \
            arr[end] = tmp;                                                                        \
            sift_down_##name(arr, end, 0);                                                         \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static usize median3_##name(const T* arr, usize i, usize j, usize k) {                         \
        if (cmp_fn(arr[i], arr[j])) {                                                              \
            if (cmp_fn(arr[j], arr[k])) return j;                                                  \
            return cmp_fn(arr[i], arr[k]) ? k : i;                                                 \
        } else {                                                                                   \
            if (cmp_fn(arr[i], arr[k])) return i;                                                  \
            return cmp_fn(arr[j], arr[k]) ? k : i;                                                 \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static usize partition_##name(T* arr, usize lo, usize hi) {                                    \
        usize mid = lo + (hi - lo) / 2;                                                            \
        T pivot = arr[median3_##name(arr, lo, mid, hi)];                                           \
        usize i = lo - 1;                                                                          \
        usize j = hi + 1;                                                                          \
                                                                                                   \
        while (true) {                                                                             \
            while (cmp_fn(arr[++i], pivot)) {}                                                     \
            while (cmp_fn(pivot, arr[--j])) {}                                                     \
            if (i >= j) return j;                                                                  \
                                                                                                   \
            T tmp = arr[i];                                                                        \
            arr[i] = arr[j];                                                                       \
            arr[j] = tmp;                                                                          \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void intro_sort_##name(T* arr, usize lo, usize hi, u16 depth) {                         \
        while (hi > lo) {                                                                          \
            usize len = hi - lo + 1;                                                               \
                                                                                                   \
            if (len < 16) {                                                                        \
                insertion_sort_##name(arr + lo, len);                                              \
                return;                                                                            \
            } else if (depth == 0) {                                                               \
                heap_sort_##name(arr + lo, len);                                                   \
                return;                                                                            \
            }                                                                                      \
                                                                                                   \
            usize p = partition_##name(arr, lo, hi);                                               \
            if (p - lo < hi - (p + 1)) {                                                           \
                intro_sort_##name(arr, lo, p, depth - 1);                                          \
                lo = p + 1;                                                                        \
            } else {                                                                               \
                intro_sort_##name(arr, p + 1, hi, depth - 1);                                      \
                hi = p;                                                                            \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void sort_unstable_##name(T* arr, usize len) {                                                 \
        if (len <= 1) return;                                                                      \
        u16 depth = log2_u64(len) * 2;                                                             \
        intro_sort_##name(arr, 0, len - 1, depth);                                                 \
    }
#endif
