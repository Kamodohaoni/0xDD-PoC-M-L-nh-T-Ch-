/* ============================================================
 *  MAY AO SYM-VM (SYM Virtual Machine) — phien ban 1.0
 * ------------------------------------------------------------
 *  May ao chay ma lenh SYM (*.sym) — ngon ngu lenh kieu tieng
 *  Viet do nguoi dung tu tao.  Tep .sym co "chu ky" %SYM 1 o
 *  dong dau, khong chuong trinh nao khac doc duoc — chi may
 *  ao nay chay duoc.
 *
 *  Cac lenh SYM:
 *     %SYM 1                      <- chu ky (bat buoc dong dau)
 *     DAT  <bien> <giatri>        gan gia tri (so hoac "chuoi")
 *     IN   <giatri>               in va xuong dong
 *     INN  <giatri>               in khong xuong dong
 *     HOI  <bien>                 nhap so tu ban phim
 *     CONG <bien> <so>            bien = bien + so
 *     TRU  <bien> <so>            bien = bien - so
 *     NHAN <bien> <so>            bien = bien * so
 *     CHIA <bien> <so>            bien = bien / so (phan nguyen)
 *     NHAY @nhan                  nhay den nhan
 *     NEU <a> <sosanh> <b> NHAY @nhan
 *          sosanh: BANG KHONG_BANG LON_HON NHO_HON
 *                  LON_HON_BANG NHO_HON_BANG
 *     @nhan                       dinh nghia mot nhan
 *     DUNG                        ket thuc chuong trinh
 *
 *  Bien dich (Visual Studio Build Tools):
 *     chay bien_dich.bat
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define TMAX      10        /* so token toi da tren mot dong        */
#define VSTRMAX   256       /* do dai chuoi toi da                  */
#define VNAMEMAX  60        /* do dai ten bien / nhan toi da        */
#define STEPMAX   5000000L  /* gioi han buoc chay (chong treo vo han) */

/* ---------- bang lenh hop le cua SYM -------------------------- */
static const char *CMDS[] = {
    "DAT", "IN", "INN", "HOI",
    "CONG", "TRU", "NHAN", "CHIA",
    "NHAY", "NEU", "DUNG"
};

/* ---------- bao loi va thoat --------------------------------- */
static void die(int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "LOI");
    if (line >= 0) fprintf(stderr, " (dong %d)", line + 1);
    fprintf(stderr, ": ");
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

/* ---------- du lieu chuong trinh ----------------------------- */
typedef struct { char *t[TMAX]; int n; } Tok;

static char **g_line = NULL;    /* dong goc (da go \r\n)           */
static Tok   *g_tok = NULL;     /* token cua tung dong             */
static int    g_nl = 0;         /* so dong                         */
static int    g_capline = 0;

typedef struct { char name[VNAMEMAX + 1]; int line; } Lab;
static Lab *g_lab = NULL; static int g_nlab = 0, g_caplab = 0;

typedef struct { char name[VNAMEMAX + 1]; int isStr; long num; char str[VSTRMAX]; } Var;
static Var *g_var = NULL; static int g_nvar = 0, g_capvar = 0;

/* ---------- tien ich ------------------------------------------ */
static char *dupn(const char *s, size_t n)
{
    char *p = (char *)malloc(n + 1);
    if (!p) die(-1, "Het bo nho.");
    memcpy(p, s, n); p[n] = 0;
    return p;
}

static int eqi(const char *a, const char *b)   /* so sanh khong phan biet hoa/thuong */
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int is_ident(const char *s)  /* ten bien/nhan: chu cai, so, gach duoi */
{
    if (!*s) return 0;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && *s != '_') return 0;
    return 1;
}

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return s;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                     e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = 0;
    return s;
}

/* ---------- tokenizer hieu dau nhay "..." ---------------------- */
/* Cat theo khoang trang, giu nguyen chuoi "co dau cach" ben trong,
   va bo comment bat dau bang # (neu khong nam trong dau nhay).   */
static void tokenize(const char *src, Tok *tk, int lineno)
{
    const char *p = src;
    tk->n = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') break;    /* gap comment -> bo phan con lai */
        if (tk->n >= TMAX)
            die(lineno, "Qua nhieu tu tren mot dong (toi da %d).", TMAX);
        if (*p == '"') {
            const char *st = p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;  /* de lau \\ va \" */
                p++;
            }
            if (*p != '"')
                die(lineno, "Chuoi trong dau nhay \"...\" chua duoc dong.");
            p++;                              /* lay luon dau nhay dong */
            tk->t[tk->n++] = dupn(st, (size_t)(p - st));
        } else {
            const char *st = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '#') p++;
            tk->t[tk->n++] = dupn(st, (size_t)(p - st));
        }
    }
}

/* ---------- nhan (label) --------------------------------------- */
static Lab *lab_find(const char *name)
{
    int i;
    for (i = 0; i < g_nlab; i++)
        if (strcmp(g_lab[i].name, name) == 0) return &g_lab[i];
    return NULL;
}

static void lab_add(const char *name, int line, int lineno)
{
    if (lab_find(name))
        die(lineno, "Nhan '%s' bi dinh danh hai lan.", name);
    if (g_nlab == g_caplab) {
        g_caplab = g_caplab ? g_caplab * 2 : 64;
        g_lab = (Lab *)realloc(g_lab, g_caplab * sizeof(Lab));
        if (!g_lab) die(-1, "Het bo nho.");
    }
    strcpy(g_lab[g_nlab].name, name);
    g_lab[g_nlab].line = line;
    g_nlab++;
}

static Lab *lab_need(const char *name, int lineno)
{
    Lab *L = lab_find(name);
    if (!L) die(lineno, "Khong tim thay nhan '@%s'.", name);
    return L;
}

/* ---------- bien (variable) ------------------------------------ */
static Var *var_find(const char *name)
{
    int i;
    for (i = 0; i < g_nvar; i++)
        if (strcmp(g_var[i].name, name) == 0) return &g_var[i];
    return NULL;
}

static Var *var_get(const char *name, int lineno)
{
    Var *v = var_find(name);
    if (!v) die(lineno, "Bien '%s' chua duoc tao. Hay dung DAT truoc.", name);
    return v;
}

static Var *var_make(const char *name, int lineno)
{
    Var *v = var_find(name);
    if (v) return v;
    if (!is_ident(name))
        die(lineno, "Ten bien '%s' khong hop le (chi chu cai / so / gach duoi).", name);
    if (strlen(name) > VNAMEMAX)
        die(lineno, "Ten bien qua dai (toi da %d).", VNAMEMAX);
    if (g_nvar == g_capvar) {
        g_capvar = g_capvar ? g_capvar * 2 : 64;
        g_var = (Var *)realloc(g_var, g_capvar * sizeof(Var));
        if (!g_var) die(-1, "Het bo nho.");
    }
    v = &g_var[g_nvar++];
    memset(v, 0, sizeof *v);
    strcpy(v->name, name);
    return v;
}

/* ---------- gia tri (operand) ---------------------------------- */
typedef struct { int isStr; long num; char str[VSTRMAX]; } Val;

/* Giai ma mot token thanh gia tri: so / "chuoi" / bien           */
static void opval(const char *tk, int lineno, Val *v)
{
    if (tk[0] == '"') {                          /* chuoi trong nhay */
        char out[VSTRMAX];
        size_t o = 0;
        const char *p = tk + 1;
        while (*p && *p != '"') {
            char c = *p;
            if (c == '\\' && p[1]) {
                p++;
                switch (*p) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case '\\':
                case '"':  c = *p;   break;
                default:   die(lineno, "Ky tu dac biet \\%c khong hop le.", *p);
                }
            }
            if (o >= VSTRMAX - 1)
                die(lineno, "Chuoi qua dai (toi da %d).", VSTRMAX - 1);
            out[o++] = c;
            p++;
        }
        out[o] = 0;
        v->isStr = 1; v->num = 0;
        strcpy(v->str, out);
        return;
    }
    /* neu la so */
    {
        const char *p = tk;
        int ok = 1;
        if (*p == '-' || *p == '+') p++;
        if (!*p) ok = 0;
        for (; *p; p++)
            if (!isdigit((unsigned char)*p)) { ok = 0; break; }
        if (ok) { v->isStr = 0; v->num = strtol(tk, NULL, 10); return; }
    }
    /* neu la bien */
    {
        Var *w = var_get(tk, lineno);
        v->isStr = w->isStr;
        v->num = w->num;
        strcpy(v->str, w->str);
        return;
    }
}

/* Doi gia tri thanh so (dung cho tinh toan / so sanh)            */
static long num_of(const Val *v, const char *who, int lineno)
{
    if (v->isStr)
        die(lineno, "'%s' can la SO nhung la chuoi.", who);
    return v->num;
}

/* Gan gia tri vao bien */
static void var_set(Var *v, const Val *nv)
{
    if (nv->isStr) {
        v->isStr = 1;
        strcpy(v->str, nv->str);
    } else {
        v->isStr = 0;
        v->num = nv->num;
    }
}

/* In gia tri ra man hinh */
static void print_val(const Val *v, int newline)
{
    if (v->isStr) printf(newline ? "%s\n" : "%s", v->str);
    else          printf(newline ? "%ld\n" : "%ld", v->num);
}

/* ---------- HOI: nhap so tu ban phim --------------------------- */
static void do_hoi(Var *v, int lineno)
{
    char buf[512];
    for (;;) {
        printf("Nhap so cho bien '%s' > ", v->name);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin))
            die(lineno, "Khong doc duoc du lieu nhap.");
        {
            char *p = trim(buf), *q = p;
            int ok = 1;
            if (*q == '-' || *q == '+') q++;
            if (!*q) ok = 0;
            for (; *q; q++)
                if (!isdigit((unsigned char)*q)) { ok = 0; break; }
            if (ok) {
                v->isStr = 0;
                v->num = strtol(p, NULL, 10);
                return;
            }
        }
        printf("  (Hay nhap mot so nguyen!)\n");
    }
}

/* ---------- thuc thi mot dong; tra ve chi muc dong ke tiep ----- */
/* Ket qua: >=0 chi dong moi; -1 nghia la DUNG (ket thuc)          */
static int exec_line(int pc)
{
    Tok *tk = &g_tok[pc];
    const char *c = tk->t[0];
    Val v;

    if (eqi(c, "DUNG")) return -1;

    if (eqi(c, "DAT")) {                             /* DAT bien giatri */
        Var *x;
        if (tk->n != 3) die(pc, "DAT can dung: DAT <bien> <giatri>");
        x = var_make(tk->t[1], pc);
        opval(tk->t[2], pc, &v);
        var_set(x, &v);
        return pc + 1;
    }

    if (eqi(c, "IN") || eqi(c, "INN")) {
        if (tk->n != 2) die(pc, "%s can dung: %s <giatri>", c, c);
        opval(tk->t[1], pc, &v);
        print_val(&v, eqi(c, "IN"));
        return pc + 1;
    }

    if (eqi(c, "HOI")) {
        if (tk->n != 2) die(pc, "HOI can dung: HOI <bien>");
        do_hoi(var_make(tk->t[1], pc), pc);
        return pc + 1;
    }

    if (eqi(c, "CONG") || eqi(c, "NHAN") || eqi(c, "TRU") || eqi(c, "CHIA")) {
        long b;
        Var *x;
        if (tk->n != 3) die(pc, "%s can dung: %s <bien> <so>", c, c);
        x = var_get(tk->t[1], pc);
        if (x->isStr)
            die(pc, "Bien '%s' la chuoi, khong tinh toan duoc.", x->name);
        opval(tk->t[2], pc, &v);
        b = num_of(&v, tk->t[2], pc);
        if (eqi(c, "CONG")) x->num += b;
        else if (eqi(c, "TRU"))  x->num -= b;
        else if (eqi(c, "NHAN")) x->num *= b;
        else {
            if (b == 0) die(pc, "Khong the CHIA cho 0.");
            x->num /= b;
        }
        x->isStr = 0;
        return pc + 1;
    }

    if (eqi(c, "NHAY")) {
        Lab *L;
        if (tk->n != 2 || tk->t[1][0] != '@')
            die(pc, "NHAY can dung: NHAY @ten_nhan");
        L = lab_need(tk->t[1] + 1, pc);
        return L->line;
    }

    if (eqi(c, "NEU")) {
        long a, b;
        int cond = 0;
        const char *op;
        Lab *L;
        if (tk->n != 6 || !eqi(tk->t[4], "NHAY") || tk->t[5][0] != '@')
            die(pc, "NEU can dung: NEU <a> <sosanh> <b> NHAY @nhan");
        opval(tk->t[1], pc, &v); a = num_of(&v, tk->t[1], pc);
        opval(tk->t[3], pc, &v); b = num_of(&v, tk->t[3], pc);
        op = tk->t[2];
        if      (eqi(op, "BANG"))         cond = (a == b);
        else if (eqi(op, "KHONG_BANG"))   cond = (a != b);
        else if (eqi(op, "LON_HON"))      cond = (a >  b);
        else if (eqi(op, "NHO_HON"))      cond = (a <  b);
        else if (eqi(op, "LON_HON_BANG")) cond = (a >= b);
        else if (eqi(op, "NHO_HON_BANG")) cond = (a <= b);
        else die(pc,
                 "Phep so sanh khong ton tai: '%s'\n"
                 "      Chi nhan: BANG KHONG_BANG LON_HON NHO_HON LON_HON_BANG NHO_HON_BANG",
                 op);
        if (cond) {
            L = lab_need(tk->t[5] + 1, pc);
            return L->line;
        }
        return pc + 1;
    }

    die(pc, "Lenh khong ton tai trong SYM-VM: '%s'", c);
}

/* ---------- tai tep .sym --------------------------------------- */
static int g_start = 0;         /* dong bat dau chay chuong trinh  */

/* Dang ky nhan + kiem tra lenh + dich nhay som */
static void setup_checks(int from)
{
    int i, k;

    /* 1. dang ky cac nhan @ten */
    for (i = 0; i < g_nl; i++) {
        Tok *tk = &g_tok[i];
        if (tk->n > 0 && tk->t[0][0] == '@') {
            if (tk->n != 1) die(i, "Nhan phai nam rieng mot dong: @ten_nhan");
            if (strlen(tk->t[0]) < 2 || strlen(tk->t[0]) - 1 > VNAMEMAX ||
                !is_ident(tk->t[0] + 1))
                die(i, "Ten nhan '%s' khong hop le.", tk->t[0]);
            lab_add(tk->t[0] + 1, i, i);
        }
    }

    /* 2. kiem tra lenh + dich nhay som de phat hien loi truoc khi chay */
    for (i = from; i < g_nl; i++) {
        Tok *tk = &g_tok[i];
        int ok = 0;
        if (tk->n == 0) continue;
        if (tk->t[0][0] == '@') continue;          /* dong nhan: bo qua */
        for (k = 0; k < (int)(sizeof CMDS / sizeof CMDS[0]); k++)
            if (eqi(tk->t[0], CMDS[k])) { ok = 1; break; }
        if (!ok)
            die(i,
                "Lenh khong ton tai trong SYM-VM: '%s'\n"
                "      Lenh hop le: DAT IN INN HOI CONG TRU NHAN CHIA NHAY NEU DUNG",
                tk->t[0]);
        if (eqi(tk->t[0], "NHAY") && tk->n == 2 && tk->t[1][0] == '@')
            lab_need(tk->t[1] + 1, i);
        if (eqi(tk->t[0], "NEU") && tk->n == 6 &&
            eqi(tk->t[4], "NHAY") && tk->t[5][0] == '@')
            lab_need(tk->t[5] + 1, i);
    }
}

static void load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    char buf[8192];
    int i, header = -1;

    if (!f) {
        fprintf(stderr, "Khong mo duoc tep: %s\n", path);
        exit(1);
    }

    /* 1. doc het tung dong */
    while (fgets(buf, sizeof buf, f)) {
        char *s = trim(buf);
        if (g_nl == g_capline) {
            g_capline = g_capline ? g_capline * 2 : 256;
            g_line = (char **)realloc(g_line, g_capline * sizeof(char *));
            g_tok  = (Tok  *)realloc(g_tok,  g_capline * sizeof(Tok));
            if (!g_line || !g_tok) die(-1, "Het bo nho.");
        }
        g_line[g_nl] = dupn(s, strlen(s));
        g_nl++;
    }
    fclose(f);
    if (g_nl == 0) die(-1, "Tep rong.");

    /* 2. token hoa tung dong */
    for (i = 0; i < g_nl; i++)
        tokenize(g_line[i], &g_tok[i], i);

    /* 3. kiem tra chu ky %SYM 1 o dong co noi dung dau tien */
    for (i = 0; i < g_nl; i++)
        if (g_tok[i].n > 0) { header = i; break; }
    if (header < 0 ||
        !eqi(g_tok[header].t[0], "%SYM") ||
        g_tok[header].n < 2 ||
        !eqi(g_tok[header].t[1], "1"))
        die(-1,
            "Dinh dang khong hop le — day KHONG PHAI tep ma SYM.\n"
            "      Chi may ao SYM-VM moi chay duoc tep nay!\n"
            "      (Tep .sym phai bat dau bang dong chu ky: %SYM 1)");

    setup_checks(header + 1);
    g_start = header + 1;
}

/* ================================================================
 *  TE MA HEX BI MAT (*.symx)
 * ----------------------------------------------------------------
 *  Dinh dang tep nhi phan chi may ao SYM-VM giai ma duoc:
 *     4 byte dau : chu ky "SYMX"   (53 59 4D 58)
 *     1 byte     : phien ban (1)
 *     con lai    : ma lenh — moi lenh:
 *          + 1 byte ma lenh (1..11 giong bang bang CMDS; 12 = dat nhan)
 *          + cac operand, moi operand bat dau bang 1 byte kieu:
 *               1 = so nguyen (4 byte nho endi)
 *               2 = chuoi trong dau nhay (keo dai 1 byte + noi dung)
 *               3 = tu cau lenh / ten bien (keo dai 1 byte + noi dung)
 *               4 = nhan '@...' (keo dai 1 byte + noi dung)
 *     Sau byte phien ban la 4 byte SO KIEM (tong 8 bit cua du lieu ma
 *     lenh truoc khi XOR, ghi nho endi) — tep bi sua thi may bao hong.
 *     Toan bo vung ma lenh duoc XOR voi khoa XKEY — khong ai doc
 *     ra noi dung bang tay duoc, chi may ao nay giai duoc.
 * ================================================================ */

static const char *BINMAGIC = "SYMX";              /* chu ky tep bi mat */
static const unsigned char BINVER = 1;             /* phien ban dinh dang */
static const char *XKEY = "MA-BI-MAT-SYM-2026!";   /* khoa dich chuyen XOR */

/* ---------- bo dem byte de xay ma lenh ------------------------ */
typedef struct { unsigned char *d; size_t n, cap; } Buf;

static void bput(Buf *b, unsigned char c)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = (unsigned char *)realloc(b->d, b->cap);
        if (!b->d) die(-1, "Het bo nho.");
    }
    b->d[b->n++] = c;
}

static void bbytes(Buf *b, const void *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) bput(b, ((const unsigned char *)p)[i]);
}

/* ---------- so operand theo ma lenh --------------------------- */
static int opcount(int opcode)
{
    switch (opcode) {
    case 1: return 2;                       /* DAT  bien gtri       */
    case 2: return 1;                       /* IN                   */
    case 3: return 1;                       /* INN                  */
    case 4: return 1;                       /* HOI                  */
    case 5: case 6: case 7: case 8: return 2; /* CONG TRU NHAN CHIA  */
    case 9: return 1;                       /* NHAY @nhan           */
    case 10: return 5;                      /* NEU a cmp b NHAY @l  */
    case 11: return 0;                      /* DUNG                 */
    case 12: return 1;                      /* @nhan                */
    }
    return -1;
}

static int cmd_index(const char *tok)
{
    int k;
    for (k = 0; k < (int)(sizeof CMDS / sizeof CMDS[0]); k++)
        if (eqi(tok, CMDS[k])) return k;
    return -1;
}

static int all_digits(const char *s)
{
    const char *p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    for (; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

/* ---------- MA HOA (bien .sym -> .symx) ------------------------ */

/* Phat 1 token ra ma lenh theo kieu cua no */
static void emit_tok(Buf *b, const char *tk, int lineno)
{
    size_t len;
    long v;

    if (tk[0] == '"') {                     /* chuoi trong nhay */
        len = strlen(tk);
        if (len > 255) die(lineno, "Chuoi qua dai de ma hoa (toi da 255 ky tu).");
        bput(b, 2);
        bput(b, (unsigned char)len);
        bbytes(b, tk, len);
        return;
    }
    if (all_digits(tk)) {                   /* so nguyen: 4 byte */
        v = strtol(tk, NULL, 10);
        bput(b, 1);
        bput(b, (unsigned char)(v & 0xFF));
        bput(b, (unsigned char)((v >> 8) & 0xFF));
        bput(b, (unsigned char)((v >> 16) & 0xFF));
        bput(b, (unsigned char)((v >> 24) & 0xFF));
        return;
    }
    if (tk[0] == '@') {                     /* nhan */
        len = strlen(tk);
        if (len > 255) die(lineno, "Ten nhan qua dai de ma hoa.");
        bput(b, 4);
        bput(b, (unsigned char)len);
        bbytes(b, tk, len);
        return;
    }
    /* con lai: tu cau lenh (NHAY, BANG...) hoac ten bien */
    len = strlen(tk);
    if (len > 255) die(lineno, "Tu '%s' qua dai de ma hoa.", tk);
    bput(b, 3);
    bput(b, (unsigned char)len);
    bbytes(b, tk, len);
}

static void encode_write(const char *outpath)
{
    FILE *f;
    Buf b;
    size_t i, j;
    unsigned long g_sum_out = 0;

    memset(&b, 0, sizeof b);

    /* xay ma lenh tu chuong trinh da tai (bo qua chu ky %SYM 1) */
    for (i = 0; i < (size_t)g_nl; i++) {
        Tok *tk = &g_tok[i];
        size_t ti;
        int k;
        if ((int)i < g_start) continue;     /* truoc/gom chu ky: bo qua */
        if (tk->n == 0) continue;
        if (tk->t[0][0] == '@') {           /* dong nhan */
            bput(&b, 12);
            emit_tok(&b, tk->t[0], (int)i);
            continue;
        }
        k = cmd_index(tk->t[0]);
        if (k < 0) die((int)i, "Lenh khong hop le: '%s'", tk->t[0]);
        bput(&b, (unsigned char)(k + 1));
        for (ti = 0; ti + 1 < (size_t)tk->n; ti++)
            emit_tok(&b, tk->t[ti + 1], (int)i);
    }

    /* so kiem tinh tren du lieu BAN RO (truoc khi XOR) */
    {
        unsigned long sum = 0;
        size_t s;
        for (s = 0; s < b.n; s++) sum += (unsigned long)b.d[s];
        g_sum_out = sum;
    }

    /* XOR toan bo vung ma lenh voi khoa bi mat */
    for (j = 0; j < b.n; j++)
        b.d[j] ^= (unsigned char)XKEY[j % strlen(XKEY)];

    f = fopen(outpath, "wb");
    if (!f) {
        fprintf(stderr, "Khong ghi duoc tep: %s\n", outpath);
        exit(1);
    }
    {
        unsigned long sum = g_sum_out;
        fwrite(BINMAGIC, 1, 4, f);
        fputc((int)BINVER, f);
        fputc((int)(sum & 0xFF), f);
        fputc((int)((sum >> 8) & 0xFF), f);
        fputc((int)((sum >> 16) & 0xFF), f);
        fputc((int)((sum >> 24) & 0xFF), f);
    }
    fwrite(b.d, 1, b.n, f);
    fclose(f);
    free(b.d);

    printf("OK! Da ma hoa thanh tep bi mat: %s\n", outpath);
    printf("   Muoi lenh / du lieu: %u byte.\n", (unsigned)b.n);
    printf("   Chi may ao SYM-VM moi giai ma va chay duoc tep nay:\n");
    printf("      mayao %s\n", outpath);
}

/* ---------- GIAI MA (chay .symx) ------------------------------ */

static FILE       *g_dec = NULL;   /* tep dang giai ma              */
static const char *g_decp = "";    /* ten tep do                    */
static size_t      g_x = 0;       /* vi tri trong vung XOR         */
static unsigned long g_sum = 0;    /* so kiem: tong 8 bit du lieu cho */
static unsigned long g_sum_expected = 0;

/* Doc 1 byte va XOR giu nghia voi vi tri cua no */
static int rdx(void)
{
    int c = fgetc(g_dec);
    if (c == EOF)
        die(-1, "Tep '%s' hong: du lieu cham dut giua lenh.", g_decp);
    c ^= (unsigned char)XKEY[(g_x++) % strlen(XKEY)];
    g_sum += (unsigned long)(unsigned char)c;
    return c;
}

static char *rd_token(int tag)
{
    static char b[600];
    int i, len;

    if (tag == 1) {                     /* so: 4 byte nho endi */
        char nb[32];
        long a0 = rdx(), a1 = rdx(), a2 = rdx(), a3 = rdx();
        long v = a0 | (a1 << 8) | (a2 << 16) | (a3 << 24);
        sprintf(nb, "%ld", v);
        return dupn(nb, strlen(nb));
    }
    len = rdx();
    if (len > 599) die(-1, "Tep hong: phan tu dai qua (%d).", len);
    for (i = 0; i < len; i++) b[i] = (char)rdx();
    b[len] = 0;
    return dupn(b, (size_t)len);
}

/* them 1 lenh vao danh sach chuong trinh da giai ma */
static void syn_push(Tok *tk)
{
    if (g_nl == g_capline) {
        g_capline = g_capline ? g_capline * 2 : 256;
        g_line = (char **)realloc(g_line, g_capline * sizeof(char *));
        g_tok  = (Tok  *)realloc(g_tok,  g_capline * sizeof(Tok));
        if (!g_line || !g_tok) die(-1, "Het bo nho.");
    }
    g_line[g_nl] = dupn("", 0);
    g_tok[g_nl] = *tk;
    g_nl++;
}

static int sniff_magic(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char h[4] = { 0, 0, 0, 0 };
    int same;
    if (!f) return 0;
    if (fread(h, 1, 4, f) != 4) { fclose(f); return 0; }
    fclose(f);
    same = memcmp(h, BINMAGIC, 4) == 0;
    return same;
}

static void decode_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char h[4];
    int opcode, ti, tag, ntok, ver;

    if (!f) {
        fprintf(stderr, "Khong mo duoc tep: %s\n", path);
        exit(1);
    }
    if (fread(h, 1, 4, f) != 4 || memcmp(h, BINMAGIC, 4) != 0) {
        fclose(f);
        die(-1, "Tep '%s' khong phai tep SYM da ma hoa.", path);
    }
    ver = fgetc(f);
    if (ver != (int)BINVER) {
        fclose(f);
        die(-1, ver == EOF
                ? "Tep SYM hong: khong tim thay so phien ban."
                : "Tep SYM phien ban khong tuong thich voi may ao nay.");
    }
    /* 4 byte so kiem (khong XOR) */
    {
        long c0 = fgetc(f), c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
        unsigned long chk;
        if (c0 == EOF || c1 == EOF || c2 == EOF || c3 == EOF) {
            fclose(f);
            die(-1, "Tep SYM hong: khong co so kiem.");
        }
        chk = (unsigned long)c0 | ((unsigned long)c1 << 8) |
              ((unsigned long)c2 << 16) | ((unsigned long)c3 << 24);
        g_sum_expected = chk;
    }
    g_dec = f; g_decp = path; g_x = 0; g_sum = 0;

    for (;;) {
        Tok tk;
        int raw;
        raw = fgetc(f);
        if (raw == EOF) break;
        opcode = raw ^ (unsigned char)XKEY[(g_x++) % strlen(XKEY)];
        g_sum += (unsigned long)(unsigned char)opcode;
        if (opcode < 1 || opcode > 12)
            die(-1, "Tep SYM hong: ma lenh %d khong hop le.", opcode);
        memset(&tk, 0, sizeof tk);
        ntok = opcount(opcode);
        if (opcode == 12) {             /* dong nhan */
            tag = rdx();
            if (tag != 4) die(-1, "Tep SYM hong: nhan sai kieu (%d).", tag);
            tk.t[0] = rd_token(tag);
            tk.n = 1;
        } else {
            tk.t[0] = dupn(CMDS[opcode - 1], strlen(CMDS[opcode - 1]));
            tk.n = 1;
            for (ti = 0; ti < ntok; ti++) {
                if (tk.n >= TMAX) die(-1, "Tep SYM hong: lenh qua nhieu tu.");
                tag = rdx();
                if (tag < 1 || tag > 4)
                    die(-1, "Tep SYM hong: kieu operand %d khong hop le.", tag);
                tk.t[tk.n++] = rd_token(tag);
            }
        }
        syn_push(&tk);
    }
    if (fgetc(f) != EOF)
        die(-1, "Tep SYM hong: co du lieu le o cuoi tep.");
    fclose(f);
    g_dec = NULL;

    if (g_sum != g_sum_expected)
        die(-1,
            "Tep SYM BI SUA HONG (so kiem khong dung).\n"
            "      Du lieu da bi can thiep hay tep sai — sy tu choi chay.");
    if (g_nl == 0) die(-1, "Tep SYM khong co lenh nao.");
    setup_checks(0);
    g_start = 0;
}

/* ---------- vong lap chay chinh ------------------------------- */
static void run(int start)
{
    long steps = 0;
    int pc = start;

    while (pc >= 0 && pc < g_nl) {
        Tok *tk = &g_tok[pc];
        if (tk->n == 0 || tk->t[0][0] == '@') { pc++; continue; }
        if (++steps > STEPMAX)
            die(pc, "Chuong trinh chay qua lau (qua %ld buoc) — co the la vong lap vo han.",
                STEPMAX);
        pc = exec_line(pc);
    }
    /* gap DUNG hoac het dong chuong trinh -> ket thuc binh thuong */
}

/* ---------- main ---------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("MAY AO SYM-VM  v1.1\n"
               "Cach dung:\n"
               "   mayao vidu.sym             chay ma lenh tieng Viet (.sym)\n"
               "   mayao vidu.symx            chay ma hex bi mat (.symx)\n"
               "   mayao -bien vidu.sym vidu.symx\n"
               "                              ma hoa .sym -> tep bi mat .symx\n"
               "Vi du: mayao vidu.sym\n");
        return 1;
    }

    /* che do bien dich: mayao -bien tep.sym tep.symx */
    if (eqi(argv[1], "-bien") || eqi(argv[1], "/bien")) {
        if (argc != 4) {
            printf("Cach dung: mayao -bien tep.sym tep.symx\n");
            return 1;
        }
        load_file(argv[2]);           /* tai + kiem tra ky tu ky %SYM 1 */
        encode_write(argv[3]);
        return 0;
    }

    /* che do chay: tu doc nhan biet tep tho hay tep da ma hoa */
    if (sniff_magic(argv[1]))
        decode_file(argv[1]);
    else
        load_file(argv[1]);

    run(g_start);
    return 0;
}
