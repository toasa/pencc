#include "util.h"
#include "token.h"
#include "parse.h"
#include "gen_x86.h"

char *regs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
int labelCount = 0;

void genExpr(Node *n);
void genStmt(Node *n);

int calcOffset(char c) {
    return (c - 'a' + 1) * 8;
}

void genIdent(Node *n) {
    printf("        mov rax, [rbp - %d]\n", n->offset);
    printf("        push rax\n");
}

void genAssign(Node *n) {
    genExpr(n->rhs);
    int offset = n->lhs->offset;
    printf("        pop rax\n");
    printf("        mov [rbp - %d], rax\n", offset);
    printf("        push rax\n");
}

void genExpr(Node *n) {
    if (n->type == ND_ASSIGN) {
        genAssign(n);
        return;
    } else if (n->type == ND_ADDR) {
        printf("        mov rax, rbp\n");
        printf("        sub rax, %d\n", n->lhs->offset);
        printf("        push rax\n");
        return;
    } else if (n->type == ND_DEREF) {
        genExpr(n->lhs);
        printf("        pop rax\n");
        printf("        mov rax, [rax]\n");
        printf("        push rax\n");
        return;
    }

    if (n->lhs) { 
        genExpr(n->lhs);
    }
    if (n->rhs) {
        genExpr(n->rhs);
    }

    if (n->type == ND_INT) {
        printf("        push %d\n", n->val);
    } else if (n->type == ND_IDENT) {
        genIdent(n);
    } else if (n->type == ND_CALL) {
        int i = 0;
        Node *cur = n->next;
        while (cur) {
            genExpr(cur);
            printf("        pop rax\n");
            printf("        mov %s, rax\n", regs[i]);
            cur = cur->next;
            i++;
        }

        printf("        mov rax, 0\n");
        printf("        call %s\n", n->func);
        printf("        push rax\n");
    } else {
        printf("        pop rdi\n");
        printf("        pop rax\n");
        if (n->type == ND_ADD) {
            printf("        add rax, rdi\n");
        } else if (n->type == ND_SUB) {
            printf("        sub rax, rdi\n");
        } else if (n->type == ND_MUL) {
            printf("        mul rdi\n");
        } else if (n->type == ND_DIV) {
            printf("        cqo\n");
            // idivはdivの符号ありバージョン
            printf("        idiv rdi\n");
        } else if (n->type == ND_REM) {
            printf("        xor rdx, rdx\n");
            // div? idiv?
            printf("        idiv rdi\n");
            // 割り算のあまりは自動的にrdxに格納される
            printf("        mov rax, rdx\n");
        } else if (n->type == ND_LSHIFT) {
            // clはrcxの下位8bit
            printf("        mov rcx, rdi\n");
            printf("        shl rax, cl\n");
        } else if (n->type == ND_RSHIFT) {
            printf("        mov rcx, rdi\n");
            printf("        sar rax, cl\n");
        } else if (n->type == ND_EQ) {
            printf("        cmp rax, rdi\n");
            printf("        sete al\n");
            printf("        movzb rax, al\n");
        } else if (n->type == ND_NQ) {
            printf("        cmp rax, rdi\n");
            printf("        setne al\n");
            printf("        movzb rax, al\n");
        } else if (n->type == ND_LT) {
            printf("        cmp rax, rdi\n");
            printf("        setl al\n");
            printf("        movzb rax, al\n");
        } else if (n->type == ND_LE) {
            printf("        cmp rax, rdi\n");
            printf("        setle al\n");
            printf("        movzb rax, al\n");
        } else if (n->type == ND_PREINC) {
            int offset = n->lhs->offset;
            printf("        add rax, rdi\n");
            printf("        mov [rbp - %d], rax\n", offset);
            printf("        mov rax, [rbp - %d]\n", offset);
        } else if (n->type == ND_PREDEC) {
            int offset = n->lhs->offset;
            printf("        sub rax, rdi\n");
            printf("        mov [rbp - %d], rax\n", offset);
            printf("        mov rax, [rbp - %d]\n", offset);     
        } else if (n->type == ND_POSTINC) {
            int offset = n->lhs->offset;
            printf("        mov rdx, rax\n");
            printf("        add rax, rdi\n");
            printf("        mov [rbp - %d], rax\n", offset);
            printf("        mov rax, rdx\n");     
        } else if (n->type == ND_POSTDEC) {
            int offset = n->lhs->offset;
            printf("        mov rdx, rax\n");
            printf("        sub rax, rdi\n");
            printf("        mov [rbp - %d], rax\n", offset);
            printf("        mov rax, rdx\n");     
        }
        printf("        push rax\n");
    }
}

void genIfStmt(Node *n) {
    genExpr(n->cond);
    printf("        pop rax\n");
    printf("        cmp rax, 0\n");
    printf("        je .L_ELSE_%03d\n", labelCount);
    genStmt(n->cons);
    printf("        jmp .L_IF_END_%03d\n", labelCount);
    printf(".L_ELSE_%03d:\n", labelCount);
    if (n->alt) {
        genStmt(n->alt);
    }
    printf(".L_IF_END_%03d:\n", labelCount);
    printf("        pop rax\n");
    labelCount++;
}

void genWhileStmt(Node *n) {
    printf(".L_WHILE_START_%03d:\n", labelCount);
    genExpr(n->cond);
    printf("        pop rax\n");
    printf("        cmp rax, 0\n");
    printf("        je .L_WHILE_END_%03d\n", labelCount);
    genStmt(n->expr);
    printf("        jmp .L_WHILE_START_%03d\n", labelCount);
    printf(".L_WHILE_END_%03d:\n", labelCount);
    printf("        pop rax\n");
    labelCount++;
}

void genForStmt(Node *n) {
    genExpr(n->init);
    printf(".L_FOR_START_%03d:\n", labelCount);
    genExpr(n->cond);
    printf("        pop rax\n");
    printf("        cmp rax, 0\n");
    printf("        je .L_FOR_END_%03d\n", labelCount);
    genStmt(n->expr);
    genExpr(n->post);
    printf("        jmp .L_FOR_START_%03d\n", labelCount);
    printf(".L_FOR_END_%03d:\n", labelCount);
    printf("        pop rax\n");
    labelCount++;
}

void genStmt(Node *n) {
    if (n->type == ND_RETURN) {
        genExpr(n->expr);
        printf("        pop rax\n");
        printf("        mov rsp, rbp\n");
        printf("        pop rbp\n");
        printf("        ret\n");
    } else if (n->type == ND_IF) {
        genIfStmt(n);
    } else if (n->type == ND_WHILE) {
        genWhileStmt(n);
    } else if (n->type == ND_FOR) {
        genForStmt(n);
    } else if (n->type == ND_BLOCK) {
        Node *cur = n->next;
        while (cur) {
            genStmt(cur);
            cur = cur->next;
        }
    } else {
        genExpr(n);
        printf("        pop rax\n");
    }
}

void genFunc(Node *func, int ident_num) {
    printf("        push rbp\n");
    printf("        mov rbp, rsp\n");

    // 関数呼び出し(call命令)の際、rspが16の倍数になっている必要がある
    if (ident_num % 2 == 1) {
        printf("        sub rsp, %d\n", (ident_num + 1) * 8);
    } else {
        printf("        sub rsp, %d\n", ident_num * 8);
    }
    
    // 現状では関数の引数を、関数内のローカル変数と同等に扱っているため
    // rbpからのネガティブのオフセットで、引数にアクセスする。
    // これは本来おかしいかも。プラスのオフセットでアクセスすべき？
    int args_i;
    for (args_i = 0; args_i < func->argsNum; args_i++) {
        printf("        mov [rbp - %d], %s\n", (args_i + 1) * 8, regs[args_i]);
    }

    genStmt(func->expr);

    printf("        mov rsp, rbp\n");
    printf("        pop rbp\n");
    printf("        ret\n\n");
}

void gen(FuncData **funcs) {
    int i;
    for (i = 0; funcs[i]; i++) {
        printf("%s:\n", funcs[i]->topLevelFunc->func);
        genFunc(funcs[i]->topLevelFunc, funcs[i]->identNum);
    }
}

void genGlobalFuncs(FuncData **funcs) {
    int i;
    for (i = 0; funcs[i]; i++) {
        printf(".global %s\n", funcs[i]->topLevelFunc->func);
    }
    printf("\n");
}

void genAssembly(FuncData **funcs) {
    printf(".intel_syntax noprefix\n");

    genGlobalFuncs(funcs);

    gen(funcs);
}