#pragma once
#include "common.h"

#define SORT_DECL(T) SORT_DECL_RAW(T, T)

#define SORT_DECL_RAW(T, Self) void sort_unstable_##Self(T* arr, usize len, void* userdata);

SORT_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

#define SORT_IMPL(T, cmp_fn) SORT_IMPL_RAW(T, T, cmp_fn)

#define SORT_IMPL_RAW(T, Self, cmp_fn)                                                              \
                                                                                                    \
    static void insertion_sort_##Self(T* arr, usize len, void* userdata) {                          \
        for (usize i = 1; i < len; ++i) {                                                           \
            T key = arr[i];                                                                         \
            usize j = i;                                                                            \
                                                                                                    \
            while (j > 0 && cmp_fn(key, arr[j - 1], userdata)) {                                    \
                arr[j] = arr[j - 1];                                                                \
                j--;                                                                                \
            }                                                                                       \
            arr[j] = key;                                                                           \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static void sift_down_##Self(T* arr, usize len, usize node, void* userdata) {                   \
        while (true) {                                                                              \
            usize child = node * 2 + 1; /* left child */                                            \
            if (child >= len) break;                                                                \
            if (child + 1 < len && cmp_fn(arr[child], arr[child + 1], userdata)) child++;           \
                                                                                                    \
            if (cmp_fn(arr[node], arr[child], userdata)) {                                          \
                T tmp = arr[node];                                                                  \
                arr[node] = arr[child];                                                             \
                arr[child] = tmp;                                                                   \
                node = child;                                                                       \
            } else {                                                                                \
                break;                                                                              \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static void heap_sort_##Self(T* arr, usize len, void* userdata) {                               \
        if (len <= 1) return;                                                                       \
                                                                                                    \
        for (usize i = len / 2; i-- > 0;) {                                                         \
            sift_down_##Self(arr, len, i, userdata);                                                \
        }                                                                                           \
        for (usize end = len - 1; end > 0; --end) {                                                 \
            T tmp = arr[0];                                                                         \
            arr[0] = arr[end];                                                                      \
            arr[end] = tmp;                                                                         \
            sift_down_##Self(arr, end, 0, userdata);                                                \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static usize median3_##Self(const T* arr, usize i, usize j, usize k, void* userdata) {          \
        if (cmp_fn(arr[i], arr[j], userdata)) {                                                     \
            if (cmp_fn(arr[j], arr[k], userdata)) return j;                                         \
            return cmp_fn(arr[i], arr[k], userdata) ? k : i;                                        \
        } else {                                                                                    \
            if (cmp_fn(arr[i], arr[k], userdata)) return k;                                         \
            return cmp_fn(arr[j], arr[k], userdata) ? k : j;                                        \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static usize partition_##Self(T* arr, usize lo, usize hi, void* userdata) {                     \
        usize mid = lo + (hi - lo) / 2;                                                             \
        T pivot = arr[median3_##Self(arr, lo, mid, hi, userdata)];                                            \
        usize i = lo - 1;                                                                           \
        usize j = hi + 1;                                                                           \
                                                                                                    \
        while (true) {                                                                              \
            while (cmp_fn(arr[++i], pivot, userdata)) {}                                            \
            while (cmp_fn(pivot, arr[--j], userdata)) {}                                            \
            if (i >= j) return j;                                                                   \
                                                                                                    \
            T tmp = arr[i];                                                                         \
            arr[i] = arr[j];                                                                        \
            arr[j] = tmp;                                                                           \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static void intro_sort_##Self(T* arr, usize lo, usize hi, u16 depth, void* userdata) {          \
        while (hi > lo) {                                                                           \
            usize len = hi - lo + 1;                                                                \
                                                                                                    \
            if (len < 16) {                                                                         \
                insertion_sort_##Self(arr + lo, len, userdata);                                               \
                return;                                                                             \
            } else if (depth == 0) {                                                                \
                heap_sort_##Self(arr + lo, len, userdata);                                                    \
                return;                                                                             \
            }                                                                                       \
                                                                                                    \
            usize p = partition_##Self(arr, lo, hi, userdata);                                                \
            if (p - lo < hi - (p + 1)) {                                                            \
                intro_sort_##Self(arr, lo, p, depth - 1, userdata);                                           \
                lo = p + 1;                                                                         \
            } else {                                                                                \
                intro_sort_##Self(arr, p + 1, hi, depth - 1, userdata);                                       \
                hi = p;                                                                             \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    void sort_unstable_##Self(T* arr, usize len, void* userdata) {                                  \
        if (len <= 1) return;                                                                       \
        u16 depth = log2_u64(len) * 2;                                                              \
        intro_sort_##Self(arr, 0, len - 1, depth, userdata);                                        \
    }
#endif
