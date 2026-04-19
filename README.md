## General patterns and philosophy

General patterns and philosophy for writing modern and safe-ish C code. Basically everything uses
ZII, usage of invalid states is legal but detectable, `goto` should be used for cleanup in a single
block at the end of the function, where the sole return statement resides.

```c
typedef enum {
    ERR_NONE = 0,
    ERR_GENERAL,
    ERR_OUT_OF_MEMORY,
} error_t; // Global error set

error_t my_func(usize len, OUT array_u32_t* nums) {
    error_t err = ERR_NONE;                // Always start by hoping for the best
    *nums = (array_u32_t) {};

    for (usize i = 0; i < len; ++i) {
        if (array_u32_push(nums, i << 5)) {
            err = ERR_OUT_OF_MEMORY;
            goto exit;
        }

        if (i == 13) { // unlucky...
            // err = .GENERAL               // Odin equivalent as north star
            // return err, nums             // Odin equivalent as north star
            // return ERROR_GENERAL;        // WRONG! I'm missing my cleanup
            err = ERR_GENERAL;
            goto exit;
        }

        if (i == 100) { // it's big enough
            // return err, nums             // Odin equivalent as north star
            // return err;                  // WRONG! I'm missing my cleanup
            goto exit;                      // Always assume `err` is in the default state
        }
    }

exit:
    if (err) array_u32_release(nums);       // errdefer
    log_info("called my_func()\n");         // defer
    return err;                             // The only return point of the whole function
}

int main() {
    array_u32_t nums = {};
    if (my_func(20, &nums)) {
        goto exit;
    }
    log_info("got no errors in main\n");

exit:
    log_error("got error in main\n");
}
```

I can't actually write Odin, but I think that the equivalent code is something along those lines

```odin
my_func :: proc(len: u32) (err: ErrCode, nums: [dyn]u8) {
    defer if err != .OK do free(&nums)
    defer printf("called my_func()")

    for (var i := 0; i < len; i++) {
        nums = append(&nums, i << 5)

        if i == 13 {
            err = .GENERAL
            return
        }

        if i == 100 {
            return
        }
    }
}
```
