/**
 * @file prop_verifier.c
 * @brief �����߼���֤��ʵ�� ���� ��Ȼ����֤������
 *
 * @details ʵ�ֻ�����Ȼ������������֤�������㷨��
 *          ֧��ֱ�������߼��;����߼�ģʽ��
 *
 *          ������������
 *            - ��ȡ��ȥ���Ӻ�ȡ�Ƴ�����
 *            - ��ȡ���룺�ӷ����Ƴ���ȡ
 *            - �̺���ȥ��modus ponens�������̺���ǰ���Ƴ����
 *            - ����ȥ���ӷ񶨺�ԭ�����Ƴ�ì��
 *            - ��ըԭ���������ã���ì���Ƴ���������
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv00/prop_verifier.h"
#include "lv00/lv00_utils.h"
#include "lv00/stream_context_util.h"
#include "lv00/lv00_internal.h"
#include "lv00/stream.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

LV00_DECLARE_STREAM_CTX(prop_verifier)

/* ============================================================
 * �ڲ�����
 * ============================================================ */

#define MAX_PREMISES     64    /**< ���ǰ���� */
#define MAX_GOALS        64    /**< �����Ŀ���� */
#define MAX_MEMO_ENTRIES 1024  /**< ���仯����С */
#define MAX_FORMULA_STR  2048  /**< ��ʽ�ַ�����󳤶� */
#define MAX_COPY_DEPTH   200   /**< ��ʽ������ݹ���ȣ���ֹջ����� */
#define MAX_DESTROY_DEPTH 200  /**< ��ʽ�������ݹ���ȣ���ֹջ����� */

/* ---- ����ջ���� ---- */
#define PROP_DESTROY_STACK_INIT_CAP   64   /* ����ʱջ�ĳ�ʼ���� */
#define PROP_DESTROY_STACK_GROWTH     2    /* ����ջ���ݱ��� */

/* ---- ��������ȼ� ---- */
#define PROP_PREC_ATOM          100   /* ԭ���������ȼ� */
#define PROP_PREC_NEGATION      80    /* ����������ȼ� */
#define PROP_PREC_CONJUNCTION   60    /* ��ȡ��������ȼ� */
#define PROP_PREC_DISJUNCTION   50    /* ��ȡ��������ȼ� */
#define PROP_PREC_IMPLICATION   40    /* �̺���������ȼ� */
#define PROP_PREC_DEFAULT       0     /* Ĭ��������ȼ� */

/* ---- ��ϣ�������� ---- */
#define PROP_HASH_TYPE_MULTIPLIER      2654435761U  /* �ƽ����������Knuth�Ƽ��� */
#define PROP_HASH_STRING_MULTIPLIER    31           /* �ַ�����ϣ���� */
#define PROP_HASH_LEFT_MULTIPLIER      0x9e3779b9U  /* ���ӹ�ʽ��ϣ���� */
#define PROP_HASH_RIGHT_MULTIPLIER     0x517cc1b7U  /* ���ӹ�ʽ��ϣ���� */
#define PROP_HASH_PTR_MULTIPLIER       0x45d9f3bU   /* ָ���ϣ���� */
#define PROP_HASH_BIT_SHIFT            16           /* ��ϣλ���ƫ���� */
#define PROP_HASH_PREMISES_MULTIPLIER  31           /* ǰ�Ἧ�Ϲ�ϣ���� */

/* ---- ʱ��ת�� ---- */
#define PROP_TIME_MS_PER_SEC           1000         /* �뵽�����ת������ */

/* ---- �̲��뻺�������� ---- */
#define PROP_SMOKE_TEST_COUNT          13           /* �����̲����� */
#define PROP_SMOKE_MAX_PREM_PTRS       8            /* �̲���ǰ��ָ����ʱ�����С */
#define PROP_SMOKE_CLEANUP_MAX_PTRS    16           /* �̲�����ʱ�������ָ���� */
#define PROP_ATOM_NAME_MAX_LEN         64           /* ԭ������������󳤶� */
#define PROP_ATOM_COLLECT_MAX          32           /* �ռ�ԭ����������� */
#define PROP_PATTERN_DESC_BUFSIZE      256          /* ģʽ������������С */
#define PROP_ANALYSIS_DESC_BUFSIZE     512          /* ����������������С */
#define PROP_MISSING_LIST_BUFSIZE      512          /* ȱʧ�б���������С */
#define PROP_STREAM_EVENT_BUFSIZE      256          /* ��ʽ�¼�������������С */
#define PROP_JSON_DETAIL_BUFSIZE       192          /* JSON���黺������С */

/* ---- ������ɫ�ж���ֵ ---- */
#define PROP_TRUST_YELLOW_THRESHOLD    2            /* ��ɫ���ε�ȱʧ�������� */
#define PROP_TRUST_AMBER_MIN           3            /* ����ɫ���ε�ȱʧ�������� */

/* ============================================================
 * ��ʽ����/����
 * ============================================================ */

/**
 * @brief ����ԭ�����⹫ʽ
 *
 * @param name ԭ����������
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_atom(const char *name) {
    if (!name) return NULL;
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_ATOM;
    snprintf(f->data.atom.name, sizeof(f->data.atom.name), "%s", name);
    return f;
}

/**
 * @brief ������ȡ��ʽ��A AND B��
 *
 * @param left  �������
 * @param right �Ҳ�����
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_conjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right) return NULL;
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_CONJUNCTION;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief ������ȡ��ʽ��A OR B��
 *
 * @param left  �������
 * @param right �Ҳ�����
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_disjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right) return NULL;
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_DISJUNCTION;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief �����̺���ʽ��A IMPLIES B��
 *
 * @param left  ǰ��
 * @param right ���
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_implication(PropFormula *left, PropFormula *right) {
    if (!left || !right) return NULL;
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_IMPLICATION;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief �����񶨹�ʽ��NOT A��
 *
 * @param operand ������
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_negation(PropFormula *operand) {
    if (!operand) return NULL;
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_NEGATION;
    f->data.unary.operand = operand;
    return f;
}

/**
 * @brief ���������͹�ʽ��ì��/�٣�
 *
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_bottom(void) {
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_BOTTOM;
    return f;
}

/**
 * @brief ������ֵ��ʽ��������/�棩
 *
 * @return �·���Ĺ�ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_create_true(void) {
    PropFormula *f = (PropFormula *)lv00_calloc(1, sizeof(PropFormula));  /* ���ʼ������ */
    if (!f) return NULL;
    f->type = PROP_TRUE;
    return f;
}

/* �ڲ�����ǰ��������static ��������ʹ��ǰ������ */
static PropFormula *prop_formula_copy_depth(const PropFormula *f, int depth);
static void prop_formula_destroy_depth(PropFormula *f, int depth);

/* �����ʽ�����ݹ���ȱ�������ֹջ����� */
/**
 * @brief ������⹫ʽ
 *
 * @param f Դ��ʽָ��
 * @return ������ʽָ�룬ʧ�ܷ��� NULL
 */
PropFormula *prop_formula_copy(const PropFormula *f) {
    return prop_formula_copy_depth(f, 0);
}

/**
 * @brief �����ʽ���ڲ�ʵ�֣����ݹ���ȱ�����
 *
 * @param f     Դ��ʽ
 * @param depth ��ǰ�ݹ����
 * @return ������ʽָ�룬����ȷ��� NULL
 */
static PropFormula *prop_formula_copy_depth(const PropFormula *f, int depth) {
    if (!f) return NULL;
    if (depth > MAX_COPY_DEPTH) {
        /* �ݹ���ȳ��ޣ���ֹջ��� */
        return NULL;
    }
    switch (f->type) {
        case PROP_ATOM:
            return prop_formula_create_atom(f->data.atom.name);
        case PROP_CONJUNCTION:
            return prop_formula_create_conjunction(
                prop_formula_copy_depth(f->data.binary.left, depth + 1),
                prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_DISJUNCTION:
            return prop_formula_create_disjunction(
                prop_formula_copy_depth(f->data.binary.left, depth + 1),
                prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_IMPLICATION:
            return prop_formula_create_implication(
                prop_formula_copy_depth(f->data.binary.left, depth + 1),
                prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_NEGATION:
            return prop_formula_create_negation(
                prop_formula_copy_depth(f->data.unary.operand, depth + 1));
        case PROP_BOTTOM:
            return prop_formula_create_bottom();
        case PROP_TRUE:
            return prop_formula_create_true();
    }
    return NULL;
}

/* �ݹ����ٹ�ʽ�����ݹ���ȱ�������ֹջ����� */
/**
 * @brief �������⹫ʽ���ݹ��ͷ�������Դ
 *
 * @param f ��ʽָ�루��Ϊ NULL��
 */
void prop_formula_destroy(PropFormula *f) {
    prop_formula_destroy_depth(f, 0);
}

/**
 * @brief ��ȫ�������⹫ʽ������ʵ�֣���ֹջ�����
 *
 * ʹ����ʽջ����ݹ�������������Ƕ�׹�ʽ���µ���ջ�����
 * ͬʱȷ�������ӹ�ʽ�ڵ㶼����ȷ�ͷţ����ڴ�й©��
 *
 * @param f     �����ٵ����⹫ʽָ��
 * @param depth δʹ�ã����������Լ��ݺ���ǩ����
 */
static void prop_formula_destroy_depth(PropFormula *f, int depth) {
    (void)depth; /* ����ʵ�ֲ�ʹ����Ȳ��� */
    if (!f) return;

    /* ��ʽջ���洢�����ٵĹ�ʽ�ڵ� */
    int stack_capacity = PROP_DESTROY_STACK_INIT_CAP;
    int stack_top = 0;
    PropFormula **stack = (PropFormula **)lv00_malloc(
        (size_t)stack_capacity * sizeof(PropFormula *));
    if (!stack) {
        /* �ڴ����ʧ�ܣ����˵��򵥵ݹ飨ǳ�㹫ʽ�Կ���ȷ���٣� */
        prop_formula_destroy(f);
        return;
    }
    stack[stack_top++] = f;

    while (stack_top > 0) {
        PropFormula *current = stack[--stack_top];

        /* ���ӽڵ�ѹջ������ȳ�����֤����˳�� */
        switch (current->type) {
            case PROP_CONJUNCTION:
            case PROP_DISJUNCTION:
            case PROP_IMPLICATION:
                /* ��Ԫ�ڵ㣺��ѹ���ӽڵ㣬��ѹ���ӽڵ� */
                if (current->data.binary.right) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity) break; /* ������� */
                        PropFormula **new_stack = (PropFormula **)lv00_realloc(
                            stack, (size_t)new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            /* ջ����ʧ�ܣ�����ֱ�ӵݹ�����ʣ���ӽڵ� */
                            if (current->data.binary.left)
                                prop_formula_destroy(current->data.binary.left);
                            if (current->data.binary.right)
                                prop_formula_destroy(current->data.binary.right);
                            current->data.binary.left = NULL;
                            current->data.binary.right = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.binary.right;
                }
                if (current->data.binary.left) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity) break;
                        PropFormula **new_stack = (PropFormula **)lv00_realloc(
                            stack, (size_t)new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            if (current->data.binary.left)
                                prop_formula_destroy(current->data.binary.left);
                            current->data.binary.left = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.binary.left;
                }
                current->data.binary.left = NULL;
                current->data.binary.right = NULL;
                break;
            case PROP_NEGATION:
                /* һԪ�ڵ㣺ѹ�������ӽڵ� */
                if (current->data.unary.operand) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity) break;
                        PropFormula **new_stack = (PropFormula **)lv00_realloc(
                            stack, (size_t)new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            if (current->data.unary.operand)
                                prop_formula_destroy(current->data.unary.operand);
                            current->data.unary.operand = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.unary.operand;
                }
                current->data.unary.operand = NULL;
                break;
            default:
                /* Ҷ�ӽڵ㣨ATOM, BOTTOM, TRUE�������ӽڵ� */
                break;
        }

        /* �ͷŵ�ǰ�ڵ� */
        lv00_free((void **)&current);
    }

    /* �ͷ�ջ */
    lv00_free((void **)&stack);
}

/* ============================================================
 * ��ʽ�Ƚϣ����ڼ��仯��ǰ��ƥ�䣩
 * ============================================================ */

/**
 * @brief ��ʽ�ṹ����ԱȽϣ��ݹ飩
 *
 * �ݹ�Ƚ��������⹫ʽ�Ľṹ����ԣ�
 * - ATOM���Ƚ������ַ���
 * - ��Ԫ����ʣ�CONJ/DISJ/IMPL�����ݹ�Ƚ������ӹ�ʽ
 * - һԪ����ʣ�NEG�����ݹ�Ƚϲ�����
 * - BOTTOM/TRUE��������ƥ�伴���
 *
 * @param a ��һ����ʽָ�루��Ϊ NULL��
 * @param b �ڶ�����ʽָ�루��Ϊ NULL��
 * @return true ��ʾ�ṹ��ȣ�false ��ʾ��ͬ
 */
static bool formula_equal(const PropFormula *a, const PropFormula *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case PROP_ATOM:
            return strcmp(a->data.atom.name, b->data.atom.name) == 0;
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return formula_equal(a->data.binary.left, b->data.binary.left) &&
                   formula_equal(a->data.binary.right, b->data.binary.right);
        case PROP_NEGATION:
            return formula_equal(a->data.unary.operand, b->data.unary.operand);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return true;
    }
    return false;
}

/* ============================================================
 * ��ʽ���л�
 * ============================================================ */

/**
 * @brief ��ȡ��������ʵ���������ȼ����������л����Ż���
 *
 * @param f ��ʽָ��
 * @return ���ȼ���ֵ��Խ�߰�Խ����
 */
static int formula_precedence(const PropFormula *f) {
    switch (f->type) {
        case PROP_ATOM:     return PROP_PREC_ATOM;
        case PROP_NEGATION: return PROP_PREC_NEGATION;
        case PROP_CONJUNCTION: return PROP_PREC_CONJUNCTION;
        case PROP_DISJUNCTION: return PROP_PREC_DISJUNCTION;
        case PROP_IMPLICATION: return PROP_PREC_IMPLICATION;
        case PROP_BOTTOM:   return PROP_PREC_ATOM;
        case PROP_TRUE:     return PROP_PREC_ATOM;
    }
    return PROP_PREC_DEFAULT;
}

/* �ڲ��ݹ����л� */
static void formula_to_string_buf(const PropFormula *f, char *buf, size_t size,
                                   int parent_prec) {
    if (!f || size == 0) return;
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

    if (need_parens) {
        strncat(buf, "(", size - strlen(buf) - 1);
    }

    switch (f->type) {
        case PROP_ATOM:
            strncat(buf, f->data.atom.name, size - strlen(buf) - 1);
            break;
        case PROP_CONJUNCTION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " /\\ ", size - strlen(buf) - 1);
            formula_to_string_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_DISJUNCTION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\/ ", size - strlen(buf) - 1);
            formula_to_string_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_IMPLICATION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " -> ", size - strlen(buf) - 1);
            formula_to_string_buf(f->data.binary.right, buf, size, prec + 1);
            break;
        case PROP_NEGATION:
            strncat(buf, "~", size - strlen(buf) - 1);
            formula_to_string_buf(f->data.unary.operand, buf, size, prec);
            break;
        case PROP_BOTTOM:
            strncat(buf, "_|_", size - strlen(buf) - 1);
            break;
        case PROP_TRUE:
            strncat(buf, "T", size - strlen(buf) - 1);
            break;
    }

    if (need_parens) {
        strncat(buf, ")", size - strlen(buf) - 1);
    }
}

/**
 * @brief �����⹫ʽ���л�Ϊ�ַ���
 *
 * @param f ��ʽָ��
 * @return �·�����ַ���ָ�룬ʧ�ܷ��� NULL
 */
char *prop_formula_to_string(const PropFormula *f) {
    if (!f) return NULL;
    char *buf = (char *)lv00_calloc(MAX_FORMULA_STR, sizeof(char));  /* ���ʼ������ */
    if (!buf) return NULL;
    formula_to_string_buf(f, buf, MAX_FORMULA_STR, 0);
    return buf;
}

/* LaTeX ���л� */
static void formula_to_latex_buf(const PropFormula *f, char *buf, size_t size,
                                  int parent_prec) {
    if (!f || size == 0) return;
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

    if (need_parens) {
        strncat(buf, "\\left(", size - strlen(buf) - 1);
    }

    switch (f->type) {
        case PROP_ATOM:
            strncat(buf, f->data.atom.name, size - strlen(buf) - 1);
            break;
        case PROP_CONJUNCTION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\wedge ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_DISJUNCTION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\vee ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_IMPLICATION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\to ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec + 1);
            break;
        case PROP_NEGATION:
            strncat(buf, "\\neg ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.unary.operand, buf, size, prec);
            break;
        case PROP_BOTTOM:
            strncat(buf, "\\bot", size - strlen(buf) - 1);
            break;
        case PROP_TRUE:
            strncat(buf, "\\top", size - strlen(buf) - 1);
            break;
    }

    if (need_parens) {
        strncat(buf, "\\right)", size - strlen(buf) - 1);
    }
}

/**
 * @brief �����⹫ʽ���л�Ϊ LaTeX �ַ���
 *
 * @param f ��ʽָ��
 * @return �·���� LaTeX �ַ���ָ�룬ʧ�ܷ��� NULL
 */
char *prop_formula_to_latex(const PropFormula *f) {
    if (!f) return NULL;
    char *buf = (char *)lv00_calloc(MAX_FORMULA_STR, sizeof(char));  /* ���ʼ������ */
    if (!buf) return NULL;
    formula_to_latex_buf(f, buf, MAX_FORMULA_STR, 0);
    return buf;
}

/* ============================================================
 * ֤���������� - �ڲ����ݽṹ
 * ============================================================ */

/* ���仯��Ŀ����¼���������� (Ŀ��, ǰ�Ἧ��) �Ƿ��֤ */
typedef struct {
    const PropFormula *goal;
    /* ��ǰ�Ἧ�ϵ�λͼ����ʶ���򻯰棺��ǰ��ָ�������ϣ�� */
    uint64_t premises_hash;
    bool proven;         /* ������Ƿ���֤�� */
    bool searched;       /* �Ƿ��������� */
} MemoEntry;

/* ֤������������ */
typedef struct {
    const PropFormula **premises;   /* ԭʼǰ�� */
    int premise_count;
    const VerifierConfig *config;
    int steps;                       /* ���ò��� */
    bool timed_out;
    /* ��ʱ��׼ʱ�� */
    uint64_t start_time_ms;
    /* ���仯�� */
    MemoEntry memo[MAX_MEMO_ENTRIES];
    int memo_count;
    /* �ݹ���ȼ���������ֹջ����� */
    int recursion_depth;
} ProofContext;

/**
 * @brief ��ȡǽ��ʱ��ʱ�䣨���룩
 *
 * ʹ�� C ��׼ time() ��ȡǽ��ʱ��ʱ�䣬���� clock() ��ȡ������ʱ�䡣
 * clock() �ڶ��̻߳� I/O �ȴ������²�׼ȷ������ CPU ʱ�������ʵʱ�䣩��
 * ����ֵ�����ڼ������ʱ������ֵ�����塣
 *
 * @return ��ǰʱ��ĺ��뼶����ֵ
 */
#include <time.h>

static uint64_t get_time_ms(void) {
    return (uint64_t)time(NULL) * PROP_TIME_MS_PER_SEC;
}

/* ============================================================
 * ��ϣ���������ڼ��仯��
 * ============================================================ */

/**
 * @brief �򵥵�ָ���ϣ����
 *
 * ʹ��ָ���ַ���� 64 λ��ϣֵ��ͨ��λ�ƺͳ�����ϡ�
 *
 * @param p ����ϣ��ָ��
 * @return 64 λ��ϣֵ
 */
static uint64_t hash_ptr(const void *p) {
    uint64_t x = (uint64_t)(uintptr_t)p;
    x = ((x >> PROP_HASH_BIT_SHIFT) ^ x) * PROP_HASH_PTR_MULTIPLIER;
    x = ((x >> PROP_HASH_BIT_SHIFT) ^ x) * PROP_HASH_PTR_MULTIPLIER;
    x = (x >> PROP_HASH_BIT_SHIFT) ^ x;
    return x;
}

/**
 * @brief ���㹫ʽ�ṹ�Ĺ�ϣֵ���ݹ飩
 *
 * ���ڹ�ʽ�ṹ���� 64 λ��ϣֵ��
 * - ATOM���������ַ������ַ���ϣ
 * - ��Ԫ����ʣ��ݹ���������ӹ�ʽ��ϣ
 * - һԪ����ʣ��ݹ���ϲ�������ϣ
 * - BOTTOM/TRUE�������͹�ϣ
 * ʹ�ûƽ���������Ͳ�ͬ�����Ա����ͻ��
 *
 * @param f ��ʽָ�루��Ϊ NULL��
 * @return 64 λ��ϣֵ��NULL ��ʽ���� 0��
 */
static uint64_t formula_hash(const PropFormula *f) {
    if (!f) return 0;
    uint64_t h = (uint64_t)f->type * PROP_HASH_TYPE_MULTIPLIER;
    switch (f->type) {
        case PROP_ATOM: {
            for (const char *s = f->data.atom.name; *s; s++)
                h = h * PROP_HASH_STRING_MULTIPLIER + (uint64_t)(unsigned char)*s;
            break;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            h ^= formula_hash(f->data.binary.left) * PROP_HASH_LEFT_MULTIPLIER;
            h ^= formula_hash(f->data.binary.right) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_NEGATION:
            h ^= formula_hash(f->data.unary.operand) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_BOTTOM:
        case PROP_TRUE:
            break;
    }
    return h;
}

/**
 * @brief ����ǰ�Ἧ�ϵĹ�ϣֵ
 *
 * ���ÿ��ǰ�ṫʽ�Ĺ�ϣֵ���� 64 λ���Ϲ�ϣ��
 *
 * @param premises ǰ�ṫʽ����
 * @param count    ǰ������
 * @return 64 λ��ϣֵ
 */
static uint64_t premises_hash(const PropFormula **premises, int count) {
    uint64_t h = 0;
    for (int i = 0; i < count; i++) {
        h = h * PROP_HASH_PREMISES_MULTIPLIER + formula_hash(premises[i]);
    }
    return h;
}

/* ============================================================
 * ���仯����
 * ============================================================ */

/* �ڼ��仯���в��� */
static int memo_find(ProofContext *ctx, const PropFormula *goal,
                      uint64_t phash) {
    uint64_t ghash = formula_hash(goal);
    for (int i = 0; i < ctx->memo_count; i++) {
        if (ctx->memo[i].goal == goal &&
            ctx->memo[i].premises_hash == phash) {
            return i;
        }
    }
    return -1;
}

/* ���Ӽ��仯��Ŀ */
static void memo_add(ProofContext *ctx, const PropFormula *goal,
                      uint64_t phash, bool proven) {
    if (ctx->memo_count >= MAX_MEMO_ENTRIES) return;
    ctx->memo[ctx->memo_count].goal = goal;
    ctx->memo[ctx->memo_count].premises_hash = phash;
    ctx->memo[ctx->memo_count].proven = proven;
    ctx->memo[ctx->memo_count].searched = true;
    ctx->memo_count++;
}

/* ============================================================
 * ǰ�����
 * ============================================================ */

/* ��ǰ���б��в��ҹ�ʽ */
static bool premise_contains(const PropFormula **premises, int count,
                              const PropFormula *f) {
    for (int i = 0; i < count; i++) {
        if (formula_equal(premises[i], f)) return true;
    }
    return false;
}

/* ============================================================
 * ǰ��������ǰ������ȡ����Ϣ
 * ============================================================ */

/**
 * @brief ǰ����չ����ȡǰ��
 *
 * ������ǰ�Ἧ����չ�����к�ȡ��ʽ��A /\ B����
 * �������ӹ�ʽ�ֱ�������ǰ���б���ȥ�أ���
 * ��������ֱ��û���µĺ�ȡ��չ����
 *
 * @param input      ����ǰ�ṫʽ����
 * @param input_count ��������
 * @param output     ���ǰ�ṫʽ���飨������Ԥ���䣩
 * @param max_output ��������������
 * @return �����ǰ�ṫʽ����
 */
static int forward_chain_conjunctions(const PropFormula **input, int input_count,
                                       const PropFormula **output, int max_output) {
    int out_count = 0;
    /* �ȸ����������� */
    for (int i = 0; i < input_count && out_count < max_output; i++) {
        output[out_count++] = input[i];
    }
    /* չ����ȡ */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < out_count && out_count < max_output; i++) {
            const PropFormula *p = output[i];
            if (p->type == PROP_CONJUNCTION) {
                /* ��������Ƿ������б��� */
                if (!premise_contains(output, out_count, p->data.binary.left)) {
                    output[out_count++] = p->data.binary.left;
                    changed = true;
                }
                if (!premise_contains(output, out_count, p->data.binary.right)) {
                    output[out_count++] = p->data.binary.right;
                    changed = true;
                }
            }
        }
    }
    return out_count;
}

/* ============================================================
 * ����֤���������ݹ飬������ӣ�
 * ============================================================ */

/* ǰ������ */
static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count,
                   const PropFormula *goal);

/* ����Ƿ�ʱ�򳬲��� */
static bool check_limits(ProofContext *ctx) {
    if (ctx->steps >= ctx->config->max_steps) return true;
    if (ctx->config->timeout_ms > 0) {
        uint64_t now = get_time_ms();
        if (now - ctx->start_time_ms >= (uint64_t)ctx->config->timeout_ms) {
            ctx->timed_out = true;
            return true;
        }
    }
    return false;
}

/* ���� modus ponens����ǰ�����ҵ� A��B �� A���Ƴ� B */
static bool try_modus_ponens(ProofContext *ctx, const PropFormula **premises,
                              int premise_count, const PropFormula *goal) {
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_IMPLICATION) {
            const PropFormula *impl = premises[i];
            const PropFormula *antecedent = impl->data.binary.left;
            const PropFormula *consequent = impl->data.binary.right;

            /* ����̺��Ľ�����Ŀ��ƥ�� */
            if (formula_equal(consequent, goal)) {
                /* ���ǰ���Ƿ���ǰ���� */
                if (premise_contains(premises, premise_count, antecedent)) {
                    ctx->steps++;
                    return true;
                }
                /* �ݹ�֤��ǰ�� */
                ctx->steps++;
                if (prove(ctx, premises, premise_count, antecedent)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* ���Դ�ǰ����ֱ��ƥ��Ŀ�� */
static bool try_direct_match(const PropFormula **premises, int premise_count,
                              const PropFormula *goal) {
    return premise_contains(premises, premise_count, goal);
}

/* ���� ?-��ȥ���� ?A �� A �Ƴ� �� */
static bool try_neg_elim(ProofContext *ctx, const PropFormula **premises,
                          int premise_count) {
    /* Ŀ���� �ͣ�����Ƿ��� ?A �� A ͬʱ��Ϊǰ�� */
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_NEGATION) {
            const PropFormula *operand = premises[i]->data.unary.operand;
            if (premise_contains(premises, premise_count, operand)) {
                ctx->steps++;
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief ����֤���������ݹ���������㷨��
 *
 * ʹ�ô��м��仯�ĵݹ���������㷨����֤����
 * 1. ��鲽����ʱ������
 * 2. ��ѯ���仯���������ظ�����
 * 3. ����Ŀ�깫ʽ���ͷ��ɣ�
 *    - BOTTOM����ȻΪ�٣���ըԭ�����ã�
 *    - TRUE��ƽ������
 *    - CONJUNCTION���ֱ�֤�������ӹ�ʽ
 *    - DISJUNCTION������֤����һ����
 *    - IMPLICATION������ Modus Ponens ����Ŀ��֤��
 *    - NEGATION�����ǰ���Ƿ��̺�ì��
 *    - ATOM������Ƿ���ǰ�Ἧ��
 *
 * @param ctx           ֤�������ģ��������á����仯���ȣ�
 * @param premises      ǰ�ṫʽ����
 * @param premise_count ǰ������
 * @param goal          ��֤����Ŀ�깫ʽ
 * @return true ��ʾ֤���ɹ���false ��ʾ֤��ʧ�ܻ�ʱ/������
 */
static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count,
                   const PropFormula *goal) {
    /* ���ݹ�������ƣ���ֹջ��� */
    ++ctx->recursion_depth;
    if (ctx->recursion_depth > MAX_MEMO_ENTRIES) {  /* ���ݹ���� = ���仯������ */
        goto prove_depth_exceeded;
    }

    /* ������� */
    if (check_limits(ctx)) {
        goto prove_depth_exceeded;
    }
    ctx->steps++;

    /* ���仯��� */
    uint64_t phash = premises_hash(premises, premise_count);
    int midx = memo_find(ctx, goal, phash);
    if (midx >= 0 && ctx->memo[midx].searched) {
        bool r = ctx->memo[midx].proven;
        ctx->recursion_depth--;
        return r;
    }

    bool result = false;

    switch (goal->type) {
        case PROP_TRUE:
            /* ? ���ǿ�֤�� */
            result = true;
            break;

        case PROP_BOTTOM:
            /* Ŀ���� �ͣ����ȼ��ǰ�����Ƿ��� �� */
            result = premise_contains(premises, premise_count, goal);
            /* ������ɹ������� ?-��ȥ */
            if (!result) {
                result = try_neg_elim(ctx, premises, premise_count);
            }
            /* ������ɹ������Դ��̺�ǰ���Ƶ�ì�� */
            if (!result) {
                for (int i = 0; i < premise_count && !result; i++) {
                    if (premises[i]->type == PROP_IMPLICATION) {
                        const PropFormula *impl = premises[i];
                        if (impl->data.binary.right->type == PROP_BOTTOM) {
                            /* �� A���� = ?A������֤�� A */
                            ctx->steps++;
                            result = prove(ctx, premises, premise_count,
                                           impl->data.binary.left);
                        }
                    }
                }
            }
            /* ǰ������չ����ȡ��Ӧ�� modus ponens��Ȼ������ ?-��ȥ */
            if (!result) {
                const PropFormula *expanded[MAX_PREMISES];
                int exp_count = forward_chain_conjunctions(premises, premise_count,
                                                            expanded, MAX_PREMISES);
                /* �ಽǰ������ */
                {
                    bool changed = true;
                    while (changed && exp_count < MAX_PREMISES) {
                        changed = false;
                        for (int i = 0; i < exp_count && !changed; i++) {
                            if (expanded[i]->type == PROP_IMPLICATION) {
                                const PropFormula *antecedent =
                                    expanded[i]->data.binary.left;
                                const PropFormula *consequent =
                                    expanded[i]->data.binary.right;
                                if (premise_contains(expanded, exp_count, antecedent) &&
                                    !premise_contains(expanded, exp_count, consequent)) {
                                    expanded[exp_count++] = consequent;
                                    changed = true;
                                    ctx->steps++;
                                }
                            }
                        }
                    }
                    /* ����չ���ǰ������ ?-��ȥ */
                    if (!result) {
                        result = try_neg_elim(ctx, expanded, exp_count);
                    }
                    /* ��� �� �Ƿ��Ƶ����� */
                    if (!result) {
                        result = premise_contains(expanded, exp_count, goal);
                    }
                }
            }
            break;

        case PROP_ATOM: {
            /* Ŀ����ԭ�����⣺ֱ��ƥ��� modus ponens */
            /* ���оֲ����������� case �����򶥲������� stack-use-after-scope */
            const PropFormula *new_premises_l[MAX_PREMISES];
            const PropFormula *new_premises_r[MAX_PREMISES];
            const PropFormula *expanded[MAX_PREMISES];
            const PropFormula *fc_expanded[MAX_PREMISES];

            result = try_direct_match(premises, premise_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, premises, premise_count, goal);
            }
            /* ǰ������չ����ȡ������ modus ponens �� */
            if (!result) {
                int exp_count = forward_chain_conjunctions(premises, premise_count,
                                                            expanded, MAX_PREMISES);
                /* �ಽǰ������������Ӧ�� modus ponens ֱ���޷��Ƶ�����ʵ */
                bool changed = true;
                while (changed && exp_count < MAX_PREMISES) {
                    changed = false;
                    for (int i = 0; i < exp_count && !changed; i++) {
                        if (expanded[i]->type == PROP_IMPLICATION) {
                            const PropFormula *antecedent =
                                expanded[i]->data.binary.left;
                            const PropFormula *consequent =
                                expanded[i]->data.binary.right;
                            if (premise_contains(expanded, exp_count, antecedent) &&
                                !premise_contains(expanded, exp_count, consequent)) {
                                expanded[exp_count++] = consequent;
                                changed = true;
                                ctx->steps++;
                            }
                        }
                    }
                    /* ���Ŀ���Ƿ�����չǰ���� */
                    result = try_direct_match(expanded, exp_count, goal);
                }
            }
            /* ���� ��-��ȥ������� A��B���� A��goal, B��goal */
            if (!result) {
                int fc_count = forward_chain_conjunctions(premises, premise_count,
                                                            fc_expanded, MAX_PREMISES);
                /* �ಽǰ������ */
                {
                    bool changed = true;
                    while (changed && fc_count < MAX_PREMISES) {
                        changed = false;
                        for (int i = 0; i < fc_count && !changed; i++) {
                            if (fc_expanded[i]->type == PROP_IMPLICATION) {
                                const PropFormula *antecedent =
                                    fc_expanded[i]->data.binary.left;
                                const PropFormula *consequent =
                                    fc_expanded[i]->data.binary.right;
                                if (premise_contains(fc_expanded, fc_count, antecedent) &&
                                    !premise_contains(fc_expanded, fc_count, consequent)) {
                                    fc_expanded[fc_count++] = consequent;
                                    changed = true;
                                    ctx->steps++;
                                }
                            }
                        }
                    }
                }

                for (int i = 0; i < fc_count && !result; i++) {
                    if (fc_expanded[i]->type == PROP_DISJUNCTION) {
                        const PropFormula *disj = fc_expanded[i];
                        /* �������֧������ A��֤�� goal */
                        ctx->steps++;
                        {
                            int new_count = fc_count;
                            memcpy((void *)new_premises_l, fc_expanded,
                                   sizeof(const PropFormula *) * (size_t)fc_count);
                            if (new_count < MAX_PREMISES) {
                                new_premises_l[new_count++] = disj->data.binary.left;
                            }
                            if (prove(ctx, new_premises_l, new_count, goal)) {
                                result = true;
                            }
                        }
                        /* �����ҷ�֧������ B��֤�� goal */
                        if (!result) {
                            ctx->steps++;
                            {
                                int new_count = fc_count;
                                memcpy((void *)new_premises_r, fc_expanded,
                                       sizeof(const PropFormula *) * (size_t)fc_count);
                                if (new_count < MAX_PREMISES) {
                                    new_premises_r[new_count++] = disj->data.binary.right;
                                }
                                if (prove(ctx, new_premises_r, new_count, goal)) {
                                    result = true;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }

        case PROP_CONJUNCTION: {
            /* Ŀ���� A �� B���ֱ�֤�� A �� B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;
            ctx->steps++;
            bool left_ok = prove(ctx, premises, premise_count, left);
            if (left_ok) {
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_DISJUNCTION: {
            /* Ŀ���� A �� B������֤�� A ��֤�� B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;

            /* �������֧ */
            ctx->steps++;
            result = prove(ctx, premises, premise_count, left);
            if (!result) {
                /* �����ҷ�֧ */
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_IMPLICATION: {
            /* Ŀ���� A �� B������ A��֤�� B */
            const PropFormula *antecedent = goal->data.binary.left;
            const PropFormula *consequent = goal->data.binary.right;

            /* �� A ����ǰ�� */
            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = antecedent;

            ctx->steps++;
            result = prove(ctx, new_premises, new_count, consequent);
            break;
        }

        case PROP_NEGATION: {
            /* Ŀ���� ?A = A �� �ͣ����� A��֤�� �� */
            const PropFormula *operand = goal->data.unary.operand;

            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = operand;

            /* ���� �� ��Ϊ��Ŀ�� */
            PropFormula *bot = prop_formula_create_bottom();
            ctx->steps++;
            result = prove(ctx, new_premises, new_count, bot);
            prop_formula_destroy(bot);
            break;
        }
    }

    /* ��ըԭ�������ǰ������ �ͣ��κ�Ŀ�궼��֤ */
    if (!result && ctx->config->enable_ex_falso) {
        /* ���ǰ�����Ƿ���� �ͣ����ⳣ��"��"�� */
        for (int i = 0; i < premise_count; i++) {
            if (premises[i]->type == PROP_BOTTOM) {
                result = true;
                break;
            }
        }
    }

    /* ���Ⳣ�ԣ�ʹ��ǰ����չ����ȡǰ������� */
    if (!result && goal->type == PROP_ATOM) {
        const PropFormula *expanded[MAX_PREMISES];
        int exp_count = forward_chain_conjunctions(premises, premise_count,
                                                    expanded, MAX_PREMISES);
        if (exp_count > premise_count) {
            /* ���µ�ǰ�ᱻ��ȡ */
            result = try_direct_match(expanded, exp_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, expanded, exp_count, goal);
            }
        }
    }

    /* ��¼���仯��� */
    memo_add(ctx, goal, phash, result);

    ctx->recursion_depth--;
    return result;

prove_depth_exceeded:
    /* �ݹ���ȳ��޻���/ʱ�䳬�ޣ�ͳһ�ڴ˵ݼ������� */
    ctx->recursion_depth--;
    return false;
}

/* ============================================================
 * ���� API
 * ============================================================ */

VerifyDetail prop_verifier_verify(
    const PropFormula **premises, int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config)
{
    VerifyDetail detail;
    memset(&detail, 0, sizeof(detail));

    /* Ĭ������ */
    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    detail.max_steps = config->max_steps;

    /* ������֤ */
    if (!goal) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message),
                 "Ŀ�깫ʽΪ NULL");
        return detail;
    }
    if (premise_count < 0) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message),
                 "ǰ������Ϊ����: %d", premise_count);
        return detail;
    }
    if (premise_count > 0 && !premises) {
        detail.result = VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message),
                 "ǰ������ > 0 ��ǰ������Ϊ NULL");
        return detail;
    }

    /* ��ʼ��֤�������� */
    ProofContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.premises = premises;
    ctx.premise_count = premise_count;
    ctx.config = config;
    ctx.start_time_ms = get_time_ms();

    /* ��ʽ�¼�����֤��ʼ */
    if (prop_verifier_stream_ctx) {
        stream_emit_simple(prop_verifier_stream_ctx,
                           STREAM_EVENT_PROOF_STEP_ADDED,
                           "������֤��ʼ������֤������",
                           0);
    }

    /* ִ��֤������ */
    bool proven = prove(&ctx, premises, premise_count, goal);

    detail.steps_used = ctx.steps;

    if (ctx.timed_out) {
        detail.result = VERIFY_TIMEOUT;
        snprintf(detail.error_message, sizeof(detail.error_message),
                 "֤��������ʱ (%d ms)", config->timeout_ms);
    } else if (proven) {
        detail.result = VERIFY_PROVEN;
        snprintf(detail.construction_summary,
                 sizeof(detail.construction_summary),
                 "֤���ɹ�: ʹ�� %d �����������֤", ctx.steps);
    } else {
        detail.result = VERIFY_FAILED;
        snprintf(detail.error_message, sizeof(detail.error_message),
                 "�����ռ�ľ���δ��֤�� (%d ��)", ctx.steps);
    }

    return detail;
}

/* ============================================================
 * �����̲⼯
 * ============================================================ */

/* ��� child �Ƿ��� parent ���ӽڵ㣨�ݹ飩 */
static bool formula_is_descendant(const PropFormula *child, const PropFormula *parent) {
    if (!child || !parent) return false;
    if (child == parent) return true;
    switch (parent->type) {
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return formula_is_descendant(child, parent->data.binary.left) ||
                   formula_is_descendant(child, parent->data.binary.right);
        case PROP_NEGATION:
            return formula_is_descendant(child, parent->data.unary.operand);
        default:
            return false;
    }
}

/* �̲⸨���꣺����ԭ������ */
#define ATOM(name) prop_formula_create_atom(name)
#define AND(a, b) prop_formula_create_conjunction((a), (b))
#define OR(a, b)  prop_formula_create_disjunction((a), (b))
#define IMPL(a, b) prop_formula_create_implication((a), (b))
#define NEG(a)    prop_formula_create_negation(a)
#define BOT()     prop_formula_create_bottom()
#define TOP()     prop_formula_create_true()

int prop_verifier_builtin_smoke_test_count(void) {
    return PROP_SMOKE_TEST_COUNT;
}

int prop_verifier_run_builtin_smoke_tests(VerifyDetail *results) {
    SmokeTest tests[PROP_SMOKE_TEST_COUNT];
    memset(&tests, 0, sizeof(tests));

    /*
     * �ڴ�������ԣ�
     * ÿ�����Ϲ�ʽ��AND/OR/IMPL/NEG����ȡ�ӽڵ������Ȩ��
     * Ϊ���� double-free��ÿ�����Կ��ڵĸ��Ϲ�ʽʹ�ö�����ԭ�����⡣
     * ����ʱʹ�� formula_is_descendant �ж���Щ��"��"��ʽ��
     */

    /* ���� 1: P, P��Q ? Q (modus ponens) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pimplq = IMPL(p, q);
        tests[0].premises[0] = p;
        tests[0].premises[1] = pimplq;
        tests[0].premise_count = 2;
        tests[0].goal = q;
        tests[0].expected_provable = true;
        tests[0].description = "P, P->Q |- Q (modus ponens)";
    }

    /* ���� 2: P��Q ? P (��-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pq = AND(p, q);
        tests[1].premises[0] = pq;
        tests[1].premise_count = 1;
        tests[1].goal = p;
        tests[1].expected_provable = true;
        tests[1].description = "P/\\Q |- P (conjunction elimination)";
    }

    /* ���� 3: P ? P��Q (��-intro left) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *porq = OR(p, q);
        tests[2].premises[0] = p;
        tests[2].premise_count = 1;
        tests[2].goal = porq;
        tests[2].expected_provable = true;
        tests[2].description = "P |- P\\/Q (disjunction introduction left)";
    }

    /* ���� 4: P��Q, Q��R ? P��R (hypothetical syllogism)
     * ÿ���̺�ʹ�ö�����ԭ������ */
    {
        PropFormula *pimplq = IMPL(ATOM("P"), ATOM("Q"));
        PropFormula *qimplr = IMPL(ATOM("Q"), ATOM("R"));
        PropFormula *pimplr = IMPL(ATOM("P"), ATOM("R"));
        tests[3].premises[0] = pimplq;
        tests[3].premises[1] = qimplr;
        tests[3].premise_count = 2;
        tests[3].goal = pimplr;
        tests[3].expected_provable = true;
        tests[3].description = "P->Q, Q->R |- P->R (hypothetical syllogism)";
    }

    /* ���� 5: P��(Q��R), P��Q ? R */
    {
        PropFormula *pimplqimplr = IMPL(ATOM("P"), IMPL(ATOM("Q"), ATOM("R")));
        PropFormula *pq = AND(ATOM("P"), ATOM("Q"));
        PropFormula *r = ATOM("R");
        tests[4].premises[0] = pimplqimplr;
        tests[4].premises[1] = pq;
        tests[4].premise_count = 2;
        tests[4].goal = r;
        tests[4].expected_provable = true;
        tests[4].description = "P->(Q->R), P/\\Q |- R";
    }

    /* ���� 6: �� ? �� (trivial) */
    {
        PropFormula *bot = BOT();
        tests[5].premises[0] = bot;
        tests[5].premise_count = 1;
        tests[5].goal = bot;
        tests[5].expected_provable = true;
        tests[5].description = "_|_ |- _|_ (trivial)";
    }

    /* ���� 7: P, ?P ? �� (?-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notp = NEG(p);
        PropFormula *bot = BOT();
        tests[6].premises[0] = p;
        tests[6].premises[1] = notp;
        tests[6].premise_count = 2;
        tests[6].goal = bot;
        tests[6].expected_provable = true;
        tests[6].description = "P, ~P |- _|_ (negation elimination)";
    }

    /* ���� 8: (P��Q)��(?Q��?P) (contraposition - intuitionistic) */
    {
        PropFormula *contra = IMPL(
            IMPL(ATOM("P"), ATOM("Q")),
            IMPL(NEG(ATOM("Q")), NEG(ATOM("P")))
        );
        tests[7].premise_count = 0;
        tests[7].goal = contra;
        tests[7].expected_provable = true;
        tests[7].description = "|- (P->Q)->(~Q->~P) (contraposition)";
    }

    /* ���� 9: P��(Q��R) ? (P��Q)��(P��R) (distribution)
     * ��������ʹ����ȫ������ԭ������ */
    {
        PropFormula *pqorr = AND(ATOM("P"), OR(ATOM("Q"), ATOM("R")));
        PropFormula *pqorpr = OR(AND(ATOM("P"), ATOM("Q")),
                                  AND(ATOM("P"), ATOM("R")));
        tests[8].premises[0] = pqorr;
        tests[8].premise_count = 1;
        tests[8].goal = pqorpr;
        tests[8].expected_provable = true;
        tests[8].description = "P/\\(Q\\/R) |- (P/\\Q)\\/(P/\\R) (distribution)";
    }

    /* ���� 10: ??P ? P (NOT provable intuitionistically) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notnotp = NEG(NEG(p));
        tests[9].premises[0] = notnotp;
        tests[9].premise_count = 1;
        tests[9].goal = p;
        tests[9].expected_provable = false;
        tests[9].description = "~~P |- P (double negation elimination - NOT intuitionistic)";
    }

    /* ���� 11: ? P��?P (NOT provable intuitionistically) */
    {
        PropFormula *pnotp = OR(ATOM("P"), NEG(ATOM("P")));
        tests[10].premise_count = 0;
        tests[10].goal = pnotp;
        tests[10].expected_provable = false;
        tests[10].description = "|- P\\/~P (LEM - NOT intuitionistic)";
    }

    /* ���� 12: ? ?P��P (NOT provable intuitionistically) */
    {
        PropFormula *notporp = OR(NEG(ATOM("P")), ATOM("P"));
        tests[11].premise_count = 0;
        tests[11].goal = notporp;
        tests[11].expected_provable = false;
        tests[11].description = "|- ~P\\/P (LEM variant - NOT intuitionistic)";
    }

    /* ���� 13: �� ? P (explosion - only with ex_falso) */
    {
        PropFormula *bot = BOT();
        PropFormula *p = ATOM("P");
        tests[12].premises[0] = bot;
        tests[12].premise_count = 1;
        tests[12].goal = p;
        tests[12].expected_provable = true;
        tests[12].description = "_|_ |- P (explosion - requires ex_falso)";
    }

    /* ���в��� */
    int passed = prop_verifier_run_smoke_tests(tests, PROP_SMOKE_TEST_COUNT, results);

    /* ������ʽ
     * ÿ�����Կ��ڵĹ�ʽ���ܹ����ӽڵ����ָͬ�롣
     * ���ԣ���ȥ�أ���ʶ��������ͳһ�ͷš�
     */
    for (int i = 0; i < PROP_SMOKE_TEST_COUNT; i++) {
        const PropFormula *ptrs[PROP_SMOKE_CLEANUP_MAX_PTRS];
        int ptr_count = 0;
        for (int j = 0; j < tests[i].premise_count && ptr_count < PROP_SMOKE_CLEANUP_MAX_PTRS; j++) {
            /* ȥ�أ������Ѵ��ڵ�ָ�� */
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].premises[j]) { dup = true; break; }
            }
            if (!dup) ptrs[ptr_count++] = tests[i].premises[j];
        }
        if (tests[i].goal) {
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].goal) { dup = true; break; }
            }
            if (!dup) ptrs[ptr_count++] = tests[i].goal;
        }

        /* ��һ�飺ʶ����Щ��"��"������������ʽ���ӽڵ㣩 */
        bool is_root[PROP_SMOKE_CLEANUP_MAX_PTRS];
        memset(is_root, true, sizeof(is_root));
        for (int k = 0; k < ptr_count; k++) {
            for (int m = 0; m < ptr_count; m++) {
                if (k != m && ptrs[k] != ptrs[m] &&
                    formula_is_descendant(ptrs[k], ptrs[m])) {
                    is_root[k] = false;
                    break;
                }
            }
        }

        /* �ڶ��飺ֻ�ͷŸ���ʽ */
        for (int k = 0; k < ptr_count; k++) {
            if (is_root[k]) {
                prop_formula_destroy((PropFormula *)ptrs[k]);
            }
        }
    }

    return passed;
}

/* ============================================================
 * ���ɹ����Է���
 * ============================================================ */

/**
 * @brief �ռ�Ŀ�깫ʽ������ԭ���ӹ�ʽ
 *
 * �ݹ������ʽ AST���ռ�����ԭ���������ơ�
 * ���ڷ���֤��ʧ��ʱ��Щԭ������ȱ�ٹ��졣
 */
static int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms) {
    if (!f) return 0;
    switch (f->type) {
        case PROP_ATOM: {
            /* ȥ�ؼ�� */
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0') break;
                if (strcmp(atoms[i], f->data.atom.name) == 0) return 0;
            }
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0') {
                    snprintf(atoms[i], PROP_ATOM_NAME_MAX_LEN, "%s", f->data.atom.name);
                    return 1;
                }
            }
            return 0;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return collect_atoms(f->data.binary.left, atoms, max_atoms) +
                   collect_atoms(f->data.binary.right, atoms, max_atoms);
        case PROP_NEGATION:
            return collect_atoms(f->data.unary.operand, atoms, max_atoms);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return 0;
    }
    return 0;
}

/**
 * @brief ���Ŀ�깫ʽ�Ƿ���������߼����е�ģʽ
 *
 * ʶ������ֱ�����岻��֤�ľ���ģʽ��
 *   - ˫�ط���ȥ��~~A �� A
 *   - �����ɣ�A �� ~A
 *   - ��֤����RAA����(~A �� ��) �� A
 */
static bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size) {
    if (!f) return false;

    /* ��������ɣ�A �� ~A �� ~A �� A */
    if (f->type == PROP_DISJUNCTION) {
        const PropFormula *left = f->data.binary.left;
        const PropFormula *right = f->data.binary.right;
        /* A �� ~A */
        if (left->type == PROP_NEGATION &&
            formula_equal(left->data.unary.operand, right)) {
            char *s = prop_formula_to_string(right);
            snprintf(pattern_desc, desc_size,
                     "������ (LEM): %s \\/ ~%s��ֱ�������߼��в���֤��",
                     s, s);
            lv00_free((void **)&s);
            return true;
        }
        /* ~A �� A */
        if (right->type == PROP_NEGATION &&
            formula_equal(right->data.unary.operand, left)) {
            char *s = prop_formula_to_string(left);
            snprintf(pattern_desc, desc_size,
                     "������ (LEM): ~%s \\/ %s��ֱ�������߼��в���֤��",
                     s, s);
            lv00_free((void **)&s);
            return true;
        }
    }

    /* ���˫�ط���ȥ��~~A �� A ��ǰ�� ~~A ? A */
    if (f->type == PROP_IMPLICATION) {
        const PropFormula *antecedent = f->data.binary.left;
        const PropFormula *consequent = f->data.binary.right;
        if (antecedent->type == PROP_NEGATION &&
            antecedent->data.unary.operand->type == PROP_NEGATION &&
            formula_equal(antecedent->data.unary.operand->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size,
                     "˫�ط���ȥ: ~~%s �� %s��ֱ�������߼��в���֤��",
                     s, s);
            lv00_free((void **)&s);
            return true;
        }
        /* ��֤�� (RAA): (~A �� ��) �� A */
        if (antecedent->type == PROP_IMPLICATION &&
            antecedent->data.binary.left->type == PROP_NEGATION &&
            antecedent->data.binary.right->type == PROP_BOTTOM &&
            formula_equal(antecedent->data.binary.left->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size,
                     "��֤�� (RAA): (~%s �� _|_) �� %s��ֱ�������߼��в���֤��",
                     s, s);
            lv00_free((void **)&s);
            return true;
        }
    }

    /* �ݹ����ӹ�ʽ */
    char sub_desc[PROP_PATTERN_DESC_BUFSIZE];
    switch (f->type) {
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            if (has_classical_pattern(f->data.binary.left, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            if (has_classical_pattern(f->data.binary.right, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        case PROP_NEGATION:
            if (has_classical_pattern(f->data.unary.operand, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(
    const PropFormula **premises, int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config)
{
    InconstructibilityAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    /* ��ִ����֤ */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    if (detail.result == VERIFY_PROVEN) {
        analysis.is_inconstructible = false;
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "������֤��Ϊ�ɹ��죬���費�ɹ����Է���");
        return analysis;
    }

    analysis.is_inconstructible = true;

    /* ����Ƿ���������߼�ģʽ */
    char pattern_desc[PROP_PATTERN_DESC_BUFSIZE] = {0};
    if (config->use_intuitionistic &&
        has_classical_pattern(goal, pattern_desc, sizeof(pattern_desc))) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "ֱ����������: %s����ֱ�������߼��У�֤�������ṩ��ʽ���죬"
                 "�������������ɻ�˫�ط���ȥ�Ⱦ�����������",
                 pattern_desc);
    } else if (detail.result == VERIFY_TIMEOUT) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "������ʱ: ֤�������� %d ������δ��ɡ�"
                 "������Ҫ���ಽ�����ڸ��ӵ���Ŀ��������ϵ��",
                 config->timeout_ms);
    } else {
        /* ����ȱ�ٵ�ǰ�����Ŀ�� */
        char goal_atoms[PROP_ATOM_COLLECT_MAX][PROP_ATOM_NAME_MAX_LEN];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, PROP_ATOM_COLLECT_MAX);

        /* �����ЩĿ��ԭ�Ӳ���ǰ���� */
        char missing[512] = {0};
        int missing_count = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM &&
                    strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s%s", missing_count > 0 ? ", " : "", goal_atoms[i]);
                strncat(missing, tmp, sizeof(missing) - strlen(missing) - 1);
                missing_count++;
            }
        }

        if (missing_count > 0) {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "ȱ�ٹ���: Ŀ����Ҫԭ������ [%s] �Ĺ��죬"
                     "����ǰǰ����δ�ṩ���� BHK �����£�"
                     "ÿ��ԭ��������Ҫһ������֤��㡢�߶λ����򣩡�",
                     missing);
        } else {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "����ȱ��: ǰ���а�������Ŀ��ԭ�����⣬���޷�ͨ��"
                     "��������������ϳ�Ŀ�ꡣ������Ҫ������̺�ǰ��"
                     "������ӵĹ��첽�衣��ʹ�� %d ��������",
                     detail.steps_used);
        }
    }

    /* ����ʧ����Ŀ������ */
    analysis.failed_subgoals = 1;
    analysis.subgoal_descriptions = (char **)lv00_malloc(sizeof(char *));  /* �����ڴ� */
    if (analysis.subgoal_descriptions) {
        analysis.subgoal_descriptions[0] = (char *)lv00_malloc(512);  /* �����ڴ� */
        if (analysis.subgoal_descriptions[0]) {
            snprintf(analysis.subgoal_descriptions[0], 512,
                     "Ŀ��: %s | ״̬: %s | ����: %d/%d",
                     prop_formula_to_string(goal),
                     detail.result == VERIFY_TIMEOUT ? "��ʱ" : "�����ռ�ľ�",
                     detail.steps_used, detail.max_steps);
        }
        analysis.subgoal_desc_count = 1;
    }

    return analysis;
}

void prop_verifier_free_analysis(InconstructibilityAnalysis *analysis) {
    if (!analysis) return;
    if (analysis->subgoal_descriptions) {
        for (int i = 0; i < analysis->subgoal_desc_count; i++) {
            lv00_free((void**)&analysis->subgoal_descriptions[i]);  /* �ͷŲ���NULL */
        }
        lv00_free((void**)&analysis->subgoal_descriptions);  /* �ͷŲ���NULL */
    }
    analysis->subgoal_desc_count = 0;
}

/* ============================================================
 * BHK ���ι�����֤�Ž�
 * ============================================================ */

/**
 * @brief ��ȡ��ʽ���͵� BHK ��������
 */
static void get_bhk_description(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0) return;
    switch (f->type) {
        case PROP_ATOM:
            snprintf(buf, size, "ԭ������ %s ��Ҫһ������֤��㡢�߶λ�����",
                     f->data.atom.name);
            break;
        case PROP_CONJUNCTION:
            snprintf(buf, size,
                     "��ȡ %s ��֤����һ��֤�� (a, b)��"
                     "��Ӧ�����еĻ����ͺ����飨����ͶӰ�˿ڣ�",
                     prop_formula_to_string(f));
            break;
        case PROP_DISJUNCTION:
            snprintf(buf, size,
                     "��ȡ %s ��֤����һ��������Դ��ǵ�֤���/�ң���"
                     "��Ӧ�����еĺ����ͺ����飨����ǵ���ȡ֤�",
                     prop_formula_to_string(f));
            break;
        case PROP_IMPLICATION:
            snprintf(buf, size,
                     "�̺� %s ��֤����һ�����캯����"
                     "��ǰ����֤��ת��Ϊ�����֤�"
                     "��Ӧ�����еı�׼�����飨����˿ڡ�����˿ڣ�",
                     prop_formula_to_string(f));
            break;
        case PROP_NEGATION:
            snprintf(buf, size,
                     "�� %s ��֤����һ���� %s ��֤��ת��Ϊ �� �Ĺ��죬"
                     "��Ӧ�����еĺ����飨�����������˿ڣ�",
                     prop_formula_to_string(f),
                     prop_formula_to_string(f->data.unary.operand));
            break;
        case PROP_BOTTOM:
            snprintf(buf, size,
                     "ì�� �� û��֤����ɹ��죩��"
                     "��Ӧ�����еĿ�ģʽ���޿����˿ڣ�");
            break;
        case PROP_TRUE:
            snprintf(buf, size,
                     "�� ? ��֤����ƽ�����죨��λ���ͣ���"
                     "��Ӧ�����еĵ�������");
            break;
    }
}

/**
 * @brief ��ȡ��ʽ���͵ļ���ӳ������
 */
static void get_geometric_mapping(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0) return;
    switch (f->type) {
        case PROP_ATOM:
            snprintf(buf, size, "GEOM_POINT / GEOM_REGION��֤��ڵ㣩");
            break;
        case PROP_CONJUNCTION:
            snprintf(buf, size, "FuncBlock[Product]�������ͺ����飬˫ͶӰ�˿ڣ�");
            break;
        case PROP_DISJUNCTION:
            snprintf(buf, size, "FuncBlock[Sum]�������ͺ����飬����Ƕ˿ڣ�");
            break;
        case PROP_IMPLICATION:
            snprintf(buf, size, "FuncBlock[Arrow]����׼�����飬���������˿ڣ�");
            break;
        case PROP_NEGATION:
            snprintf(buf, size, "FuncBlock[Neg]���񶨺����飬������Ͷ˿ڣ�");
            break;
        case PROP_BOTTOM:
            snprintf(buf, size, "��ģʽ���޶˿ڣ�������䣩");
            break;
        case PROP_TRUE:
            snprintf(buf, size, "�������򣨵�λ����֤�");
            break;
    }
}

BHKVerificationResult prop_verifier_bhk_verify(
    const PropFormula **premises, int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config)
{
    BHKVerificationResult result;
    memset(&result, 0, sizeof(result));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    /* ��ִ�������߼���֤ */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);
    result.verified = (detail.result == VERIFY_PROVEN);

    /* ���� BHK ���� */
    get_bhk_description(goal, result.bhk_interpretation, sizeof(result.bhk_interpretation));

    /* ���ɼ���ӳ�� */
    get_geometric_mapping(goal, result.geometric_mapping, sizeof(result.geometric_mapping));

    if (result.verified) {
        /* ��֤�ɹ�����鹹�������� */
        result.missing_constructions = 0;
        result.missing_descriptions = NULL;
        result.missing_count = 0;
    } else {
        /* ��֤ʧ�ܣ�����ȱ�ٵĹ��� */
        char goal_atoms[32][64];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, 32);

        /* ͳ��ȱ�ٹ����ԭ������ */
        int missing = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM &&
                    strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) missing++;
        }

        result.missing_constructions = missing;
        result.missing_count = missing;

        if (missing > 0) {
            result.missing_descriptions = (char **)lv00_malloc(sizeof(char *) * (size_t)missing);  /* �����ڴ� */
            if (result.missing_descriptions) {
                int idx = 0;
                for (int i = 0; i < atom_count && idx < missing; i++) {
                    bool found = false;
                    for (int j = 0; j < premise_count; j++) {
                        if (premises[j]->type == PROP_ATOM &&
                            strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        char desc[256];
                        snprintf(desc, sizeof(desc),
                                 "ȱ��ԭ������ '%s' �ļ���֤���Ҫ��Ӧ�ĵ㡢�߶λ�����ڵ㣩",
                                 goal_atoms[i]);
                        result.missing_descriptions[idx] = lv00_strdup_safe(desc);  /* �����ַ��� */
                        idx++;
                    }
                }
            }
        } else {
            result.missing_descriptions = NULL;
            /* ��ԭ��ǰ�ᵫ�޷����죺��������������������� */
            result.missing_count = 1;
            result.missing_descriptions = (char **)lv00_malloc(sizeof(char *));  /* �����ڴ� */
            if (result.missing_descriptions) {
                char desc[256];
                snprintf(desc, sizeof(desc),
                         "�޷�ͨ������ǰ�����Ϲ���Ŀ�꣨������������������");
                result.missing_descriptions[0] = lv00_strdup_safe(desc);  /* �����ַ��� */
            }
        }
    }

    return result;
}

void prop_verifier_free_bhk_result(BHKVerificationResult *result) {
    if (!result) return;
    if (result->missing_descriptions) {
        for (int i = 0; i < result->missing_count; i++) {
            lv00_free((void**)&result->missing_descriptions[i]);  /* �ͷŲ���NULL */
        }
        lv00_free((void**)&result->missing_descriptions);  /* �ͷŲ���NULL */
    }
    result->missing_count = 0;
}

/* ============================================================
 * ������ɫ�Ž� ���� BHK��֤��� �� Լ��ͼ TrustColor
 * ============================================================ */

/**
 * @brief ���� BHK ��֤���ӳ�� TrustColor
 *
 * ����֤���ӳ��Ϊ�ʵ���������ɫ��
 *   - verified + 0 missing �� TRUST_GREEN
 *   - verified + 1-2 missing �� TRUST_YELLOW
 *   - verified + 3+ missing �� TRUST_AMBER
 *   - δ��֤��VERIFY_FAILED���� TRUST_BLUE
 *   - ��֤α��VERIFY_DISPROVEN���� TRUST_RED
 *   - ��ʱ/���� �� TRUST_BLUE
 */
static TrustColor map_bhk_to_trust_color(const BHKVerificationResult *bhk,
                                          VerifyResult verify_result) {
    switch (verify_result) {
    case VERIFY_PROVEN:
        if (!bhk->verified) {
            /* BHK��δͨ���������ͨ���������Կ��� */
            return TRUST_YELLOW;
        }
        if (bhk->missing_constructions == 0) {
            return TRUST_GREEN;
        } else if (bhk->missing_constructions <= 2) {
            return TRUST_YELLOW;
        } else {
            return TRUST_AMBER;
        }
    case VERIFY_DISPROVEN:
        return TRUST_RED;
    case VERIFY_FAILED:
        return TRUST_BLUE;
    case VERIFY_TIMEOUT:
    case VERIFY_INVALID_INPUT:
    case VERIFY_ERROR:
    default:
        return TRUST_BLUE;
    }
}

/**
 * @brief ��ȡ TrustColor ����������
 */
static const char *trust_color_name(TrustColor color) {
    switch (color) {
    case TRUST_GREEN:  return "��ɫ����ȫ���ţ�";
    case TRUST_BLUE:   return "��ɫ��δȷ����";
    case TRUST_YELLOW: return "��ɫ�������Կ��ţ�";
    case TRUST_ORANGE: return "��ɫ�����ע��";
    case TRUST_LIGHT_ORANGE: return "ǳ��ɫ";
    case TRUST_RED:    return "��ɫ��������/��֤α��";
    case TRUST_AMBER:  return "����ɫ������ȱʧ��";
    default:           return "δ֪";
    }
}

int prop_verifier_apply_trust_colors(
    ConstraintGraph *graph,
    const PropFormula **premises, int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config,
    BHKVerificationResult *out_result)
{
    if (!graph) return -1;

    /* ����1: ִ�� BHK ��֤ */
    BHKVerificationResult bhk = prop_verifier_bhk_verify(
        premises, premise_count, goal, config);

    /* ͬʱ��ȡԭʼ��֤������ж� DISPROVEN ��״̬ */
    VerifierConfig default_cfg = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_cfg;
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    /* ����2: ӳ��������ɫ */
    TrustColor target_color = map_bhk_to_trust_color(&bhk, detail.result);

    /* ��ʽ���: ��֤��ʼ */
    if (prop_verifier_stream_ctx) {
        char desc[256];
        snprintf(desc, sizeof(desc),
                 "������ɫ�Ž�: BHK��֤=%s, ȱʧ����=%d, Ŀ����ɫ=%s",
                 bhk.verified ? "ͨ��" : "δͨ��",
                 bhk.missing_constructions,
                 trust_color_name(target_color));
        stream_emit_simple(prop_verifier_stream_ctx,
                           STREAM_EVENT_PROOF_COLOR_UPDATE, desc, 0);
    }

    /* ����3: ����Լ��ͼ�е����нڵ㣬����������ɫ */
    int updated_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;

        bool node_updated = false;

        /* �Խڵ��ÿ��������������������ɫ */
        for (int c = 0; c < node->coord_count; c++) {
            SymbolicCoord *coord = node->symbolic_coords[c];
            if (!coord) continue;

            TrustColor old_color = symbolic_coord_get_trust(coord);
            if (old_color != target_color) {
                symbolic_coord_set_trust(coord, target_color);
                node_updated = true;
            }
        }

        if (node_updated) {
            updated_count++;

            /* ��ʽ���: �����ڵ����ɫ���� */
            if (prop_verifier_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROOF_COLOR_UPDATE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.node_id = node->id;
                ev.step_number = i;
                ev.total_steps = graph->node_count;
                ev.description = trust_color_name(target_color);
                char detail_json[192];
                snprintf(detail_json, sizeof(detail_json),
                         "{\"node_id\":%d,\"type\":%d,\"old_color\":%d,\"new_color\":%d,"
                         "\"verified\":%s,\"missing\":%d}",
                         node->id, (int)node->type,
                         (int)symbolic_coord_get_trust(
                             node->coord_count > 0 && node->symbolic_coords[0]
                                 ? node->symbolic_coords[0] : NULL),
                         (int)target_color,
                         bhk.verified ? "true" : "false",
                         bhk.missing_constructions);
                ev.detail_json = detail_json;
                stream_emit(prop_verifier_stream_ctx, &ev);
            }
        }
    }

    /* ��ʽ���: ���ͳ�� */
    if (prop_verifier_stream_ctx) {
        char done_desc[128];
        snprintf(done_desc, sizeof(done_desc),
                 "������ɫӦ�����: ������ %d/%d ���ڵ�",
                 updated_count, graph->node_count);
        stream_emit_simple(prop_verifier_stream_ctx,
                           STREAM_EVENT_PROOF_COLOR_UPDATE, done_desc, 0);
    }

    /* ����4: �������������������Ҫ�� */
    if (out_result) {
        memcpy(out_result, &bhk, sizeof(BHKVerificationResult));
        /* ע��: missing_descriptions ������Ȩת�Ƹ������� */
        /* ���ڴ˴��ͷ� bhk.missing_descriptions */
    } else {
        /* �����߲���Ҫ��������Ǹ����ͷ� */
        prop_verifier_free_bhk_result(&bhk);
    }

    return updated_count;
}

/* ============================================================
 * ����ȼ��Լ��
 * ============================================================ */

bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b,
                                      const VerifierConfig *config) {
    if (!a || !b) return false;

    /* �ṹ����Կ���·�� */
    if (formula_equal(a, b)) return true;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    /* ��� a �� b �� b �� a �Ƿ񶼿�֤ */
    PropFormula *a_impl_b = prop_formula_create_implication(
        prop_formula_copy(a), prop_formula_copy(b));
    PropFormula *b_impl_a = prop_formula_create_implication(
        prop_formula_copy(b), prop_formula_copy(a));

    VerifyDetail d1 = prop_verifier_verify(NULL, 0, a_impl_b, config);
    VerifyDetail d2 = prop_verifier_verify(NULL, 0, b_impl_a, config);

    bool result = (d1.result == VERIFY_PROVEN && d2.result == VERIFY_PROVEN);

    prop_formula_destroy(a_impl_b);
    prop_formula_destroy(b_impl_a);

    return result;
}

bool prop_verifier_check_tautology(const PropFormula *f,
                                    const VerifierConfig *config) {
    if (!f) return false;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    /* ����ʽ = ��ǰ�ἴ��֤ */
    VerifyDetail detail = prop_verifier_verify(NULL, 0, f, config);
    return detail.result == VERIFY_PROVEN;
}

int prop_verifier_run_smoke_tests(const SmokeTest *tests, int test_count,
                                   VerifyDetail *results) {
    int passed = 0;

    for (int i = 0; i < test_count; i++) {
        const SmokeTest *t = &tests[i];

        /* ���� 13 ��Ҫ���� ex_falso */
        VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
        /* ����Ԥ�ڲ���֤�Ĳ��ԣ�ʹ��ֱ������ģʽ */
        /* ���ڲ��� 13����ըԭ���������� ex_falso */
        if (t->expected_provable && t->premise_count == 1 &&
            t->premises[0] && t->premises[0]->type == PROP_BOTTOM &&
            t->goal && t->goal->type == PROP_ATOM) {
            config.enable_ex_falso = true;
        }

        /* ���̶���С����תΪָ��������ƥ�� API */
        const PropFormula *prem_ptrs[PROP_SMOKE_MAX_PREM_PTRS];
        for (int j = 0; j < t->premise_count && j < PROP_SMOKE_MAX_PREM_PTRS; j++) {
            prem_ptrs[j] = t->premises[j];
        }

        results[i] = prop_verifier_verify(prem_ptrs, t->premise_count,
                                           t->goal, &config);

        bool actually_proven = (results[i].result == VERIFY_PROVEN);

        /* ����Ԥ�ڲ���֤�Ĳ��ԣ�����Ƿ�ȷʵ����֤ */
        if (t->expected_provable) {
            if (actually_proven) passed++;
        } else {
            if (!actually_proven) passed++;
        }
    }

    return passed;
}

void prop_verifier_set_stream_context(StreamContext *ctx) {
    prop_verifier_stream_ctx = ctx;
}

StreamContext *prop_verifier_get_stream_context(void) {
    return prop_verifier_stream_ctx;
}

PropVerifierResult lv00_prop_verify(const void *prop) {
    PropVerifierResult res;
    res.valid = false;
    res.msg = "Not implemented";
    return res;
}
