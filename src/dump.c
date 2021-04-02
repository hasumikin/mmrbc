#include "dump.h"

#define MRB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define MRB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define MRB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define MRB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define MRB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define MRB_ASPEC_KDICT(a)        (((a) >> 1) & 0x1)
#define MRB_ASPEC_BLOCK(a)        ((a) & 1)

#define PICOPEEK_B(irep) (*(irep))

#define PICOPEEK_S(irep) ((irep)[0]<<8|(irep)[1])
#define PICOPEEK_W(irep) ((irep)[0]<<16|(irep)[1]<<8|(irep)[2])

#define PICOREAD_B() PICOPEEK_B(irep++)
#define PICOREAD_S() (irep+=2, PICOPEEK_S(irep-2))
#define PICOREAD_W() (irep+=3, PICOPEEK_W(irep-3))

#define PICOFETCH_Z() /* nothing */
#define PICOFETCH_B()   do {a=PICOREAD_B();} while (0)
#define PICOFETCH_BB()  do {a=PICOREAD_B(); b=PICOREAD_B();} while (0)
#define PICOFETCH_BBB() do {a=PICOREAD_B(); b=PICOREAD_B(); c=PICOREAD_B();} while (0)
#define PICOFETCH_BS()  do {a=PICOREAD_B(); b=PICOREAD_S();} while (0)
#define PICOFETCH_BSS() do {a=PICOREAD_B(); b=PICOREAD_S(); c=PICOREAD_S();} while (0)
#define PICOFETCH_S()   do {a=PICOREAD_S();} while (0)
#define PICOFETCH_W()   do {a=PICOREAD_W();} while (0)

void
print_lv_a(void *dummy, uint8_t *irep, uint16_t a)
{
}
void
print_lv_ab(void *dummy, uint8_t *irep, uint16_t a, uint16_t b)
{
}

char*
mrb_sym_dump(char *s, uint16_t b)
{
  sprintf(s, "%d", b);
  return s;
}

#define CASE(insn,ops) case insn: PICOFETCH_ ## ops ();

void
Dump_codeDump(uint8_t *irep)
{
  char mrb[10];
  uint32_t len = 0;
  len += *(irep + 10) << 24;
  len += *(irep + 11) << 16;
  len += *(irep + 12) << 8;
  len += *(irep + 13);
  printf("pos: %d / len: %u\n", *irep, len);

  irep += 14; /* skip irep header */

  uint8_t *opstart = irep;

  uint8_t *irepend = irep + len;

  int i = 0; /*dummy*/

  while (irep < irepend) {
    printf("    1 %03d ", (int)(irep - opstart));
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t ins = *irep++;
    switch (ins) {
    CASE(OP_NOP, Z);
      printf("OP_NOP\n");
      break;
    CASE(OP_MOVE, BB);
      printf("OP_MOVE\tR%d\tR%d\t", a, b);
      print_lv_ab(mrb, irep, a, b);
      break;
//    CASE(OP_LOADL16, BS);
//      goto op_loadl;
//
//    CASE(OP_LOADL, BB);
//    op_loadl:
//      switch (irep->pool[b].tt) {
//      case IREP_TT_FLOAT:
//#ifndef MRB_NO_FLOAT
//        printf("OP_LOADL\tR%d\tL(%d)\t; %f", a, b, (double)irep->pool[b].u.f);
//#endif
//        break;
//      case IREP_TT_INT32:
//        printf("OP_LOADL\tR%d\tL(%d)\t; %" PRId32, a, b, irep->pool[b].u.i32);
//        break;
//#ifdef MRB_64BIT
//      case IREP_TT_INT64:
//        printf("OP_LOADL\tR%d\tL(%d)\t; %" PRId64, a, b, irep->pool[b].u.i64);
//        break;
//#endif
//      default:
//        printf("OP_LOADL\tR%d\tL(%d)\t", a, b);
//        break;
//      }
//      print_lv_a(mrb, irep, a);
//      break;
    CASE(OP_LOADI, BB);
      printf("OP_LOADI\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADINEG, BB);
      printf("OP_LOADI\tR%d\t-%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
//    CASE(OP_LOADI16, BS);
//      printf("OP_LOADI16\tR%d\t%d\t", a, (int)(int16_t)b);
//      print_lv_a(mrb, irep, a);
//      break;
//    CASE(OP_LOADI32, BSS);
//      printf("OP_LOADI32\tR%d\t%d\t", a, (int32_t)(((uint32_t)b<<16)+c));
//      print_lv_a(mrb, irep, a);
//      break;
    CASE(OP_LOADI__1, B);
      printf("OP_LOADI__1\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI_0, B); goto L_LOADI;
    CASE(OP_LOADI_1, B); goto L_LOADI;
    CASE(OP_LOADI_2, B); goto L_LOADI;
    CASE(OP_LOADI_3, B); goto L_LOADI;
    CASE(OP_LOADI_4, B); goto L_LOADI;
    CASE(OP_LOADI_5, B); goto L_LOADI;
    CASE(OP_LOADI_6, B); goto L_LOADI;
    CASE(OP_LOADI_7, B);
    L_LOADI:
      printf("OP_LOADI_%d\tR%d\t\t", ins-(int)OP_LOADI_0, a);
      print_lv_a(mrb, irep, a);
      break;
//    CASE(OP_LOADSYM16, BS);
//      goto op_loadsym;
    CASE(OP_LOADSYM, BB);
//    op_loadsym:
      printf("OP_LOADSYM\tR%d\t:%s\t", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADNIL, B);
      printf("OP_LOADNIL\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADSELF, B);
      printf("OP_LOADSELF\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADT, B);
      printf("OP_LOADT\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADF, B);
      printf("OP_LOADF\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETGV, BB);
      printf("OP_GETGV\tR%d\t:%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETGV, BB);
      printf("OP_SETGV\t:%s\tR%d", mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETSV, BB);
      printf("OP_GETSV\tR%d\t:%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETSV, BB);
      printf("OP_SETSV\t:%s\tR%d", mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETCONST, BB);
      printf("OP_GETCONST\tR%d\t:%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETCONST, BB);
      printf("OP_SETCONST\t:%s\tR%d", mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETMCNST, BB);
      printf("OP_GETMCNST\tR%d\tR%d::%s", a, a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETMCNST, BB);
      printf("OP_SETMCNST\tR%d::%s\tR%d", a+1, mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETIV, BB);
      printf("OP_GETIV\tR%d\t%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETIV, BB);
      printf("OP_SETIV\t%s\tR%d", mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETUPVAR, BBB);
      printf("OP_GETUPVAR\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETUPVAR, BBB);
      printf("OP_SETUPVAR\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETCV, BB);
      printf("OP_GETCV\tR%d\t%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETCV, BB);
      printf("OP_SETCV\t%s\tR%d", mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMP, S);
//      i = pc - irep->iseq;
      printf("OP_JMP\t\t%03d\n", (int)i+(int16_t)a);
      break;
//    CASE(OP_JMPUW, S);
////      i = pc - irep->iseq;
//      printf("OP_JMPUW\t\t%03d\n", (int)i+(int16_t)a);
//      break;
    CASE(OP_JMPIF, BS);
//      i = pc - irep->iseq;
      printf("OP_JMPIF\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMPNOT, BS);
//      i = pc - irep->iseq;
      printf("OP_JMPNOT\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMPNIL, BS);
//      i = pc - irep->iseq;
      printf("OP_JMPNIL\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SENDV, BB);
      printf("OP_SENDV\tR%d\t:%s\n", a, mrb_sym_dump(mrb, b));
      break;
    CASE(OP_SENDVB, BB);
      printf("OP_SENDVB\tR%d\t:%s\n", a, mrb_sym_dump(mrb, b));
      break;
    CASE(OP_SEND, BBB);
      printf("OP_SEND\tR%d\t:%s\t%d\n", a, mrb_sym_dump(mrb, b), c);
      break;
    CASE(OP_SENDB, BBB);
      printf("OP_SENDB\tR%d\t:%s\t%d\n", a, mrb_sym_dump(mrb, b), c);
      break;
    CASE(OP_CALL, Z);
      printf("OP_CALL\n");
      break;
    CASE(OP_SUPER, BB);
      printf("OP_SUPER\tR%d\t%d\n", a, b);
      break;
    CASE(OP_ARGARY, BS);
      printf("OP_ARGARY\tR%d\t%d:%d:%d:%d (%d)", a,
             (b>>11)&0x3f,
             (b>>10)&0x1,
             (b>>5)&0x1f,
             (b>>4)&0x1,
             (b>>0)&0xf);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ENTER, W);
      printf("OP_ENTER\t%d:%d:%d:%d:%d:%d:%d\n",
             MRB_ASPEC_REQ(a),
             MRB_ASPEC_OPT(a),
             MRB_ASPEC_REST(a),
             MRB_ASPEC_POST(a),
             MRB_ASPEC_KEY(a),
             MRB_ASPEC_KDICT(a),
             MRB_ASPEC_BLOCK(a));
      break;
    CASE(OP_KEY_P, BB);
      printf("OP_KEY_P\tR%d\t:%s\t", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_KEYEND, Z);
      printf("OP_KEYEND\n");
      break;
    CASE(OP_KARG, BB);
      printf("OP_KARG\tR%d\t:%s\t", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RETURN, B);
      printf("OP_RETURN\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RETURN_BLK, B);
      printf("OP_RETURN_BLK\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_BREAK, B);
      printf("OP_BREAK\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_BLKPUSH, BS);
      printf("OP_BLKPUSH\tR%d\t%d:%d:%d:%d (%d)", a,
             (b>>11)&0x3f,
             (b>>10)&0x1,
             (b>>5)&0x1f,
             (b>>4)&0x1,
             (b>>0)&0xf);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LAMBDA, BB);
      printf("OP_LAMBDA\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
    CASE(OP_BLOCK, BB);
      printf("OP_BLOCK\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
    CASE(OP_METHOD, BB);
      printf("OP_METHOD\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
//    CASE(OP_LAMBDA16, BS);
//      printf("OP_LAMBDA\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
//    CASE(OP_BLOCK16, BS);
//      printf("OP_BLOCK\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
//    CASE(OP_METHOD16, BS);
//      printf("OP_METHOD\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
    CASE(OP_RANGE_INC, B);
      printf("OP_RANGE_INC\tR%d\n", a);
      break;
    CASE(OP_RANGE_EXC, B);
      printf("OP_RANGE_EXC\tR%d\n", a);
      break;
    CASE(OP_DEF, BB);
      printf("OP_DEF\tR%d\t:%s\n", a, mrb_sym_dump(mrb, b));
      break;
    CASE(OP_UNDEF, B);
      printf("OP_UNDEF\t:%s\n", mrb_sym_dump(mrb, b));
      break;
    CASE(OP_ALIAS, BB);
      printf("OP_ALIAS\t:%s\t%s\n", mrb_sym_dump(mrb, b), mrb_sym_dump(mrb, b));
      break;
    CASE(OP_ADD, B);
      printf("OP_ADD\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_ADDI, BB);
      printf("OP_ADDI\tR%d\t%d\n", a, b);
      break;
    CASE(OP_SUB, B);
      printf("OP_SUB\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_SUBI, BB);
      printf("OP_SUBI\tR%d\t%d\n", a, b);
      break;
    CASE(OP_MUL, B);
      printf("OP_MUL\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_DIV, B);
      printf("OP_DIV\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_LT, B);
      printf("OP_LT\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_LE, B);
      printf("OP_LE\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_GT, B);
      printf("OP_GT\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_GE, B);
      printf("OP_GE\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_EQ, B);
      printf("OP_EQ\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_ARRAY, BB);
      printf("OP_ARRAY\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARRAY2, BBB);
      printf("OP_ARRAY\tR%d\tR%d\t%d\t", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_ARYCAT, B);
      printf("OP_ARYCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARYPUSH, B);
      printf("OP_ARYPUSH\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARYDUP, B);
      printf("OP_ARYDUP\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_AREF, BBB);
      printf("OP_AREF\tR%d\tR%d\t%d", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_ASET, BBB);
      printf("OP_ASET\tR%d\tR%d\t%d", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_APOST, BBB);
      printf("OP_APOST\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_INTERN, B);
      printf("OP_INTERN\tR%d", a);
      print_lv_a(mrb, irep, a);
      break;
//    CASE(OP_STRING16, BS);
//      goto op_string;
    CASE(OP_STRING, BB);
//    op_string:
//      if ((irep->pool[b].tt & IREP_TT_NFLAG) == 0) {
//        printf("OP_STRING\tR%d\tL(%d)\t; %s", a, b, irep->pool[b].u.str);
//      }
//      else {
        printf("OP_STRING\tR%d\tL(%d)\t", a, b);
//      }
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_STRCAT, B);
      printf("OP_STRCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASH, BB);
      printf("OP_HASH\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASHADD, BB);
      printf("OP_HASHADD\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASHCAT, B);
      printf("OP_HASHCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;

    CASE(OP_OCLASS, B);
      printf("OP_OCLASS\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_CLASS, BB);
      printf("OP_CLASS\tR%d\t:%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_MODULE, BB);
      printf("OP_MODULE\tR%d\t:%s", a, mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_EXEC, BB);
      printf("OP_EXEC\tR%d\tI(%d:%p)", a, b, NULL);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SCLASS, B);
      printf("OP_SCLASS\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_TCLASS, B);
      printf("OP_TCLASS\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ERR, B);
 //     if ((irep->pool[a].tt & IREP_TT_NFLAG) == 0) {
 //       printf("OP_ERR\t%s\n", irep->pool[a].u.str);
 //     }
 //     else {
        printf("OP_ERR\tL(%d)\n", a);
//      }
      break;
    CASE(OP_EXCEPT, B);
      printf("OP_EXCEPT\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RESCUE, BB);
      printf("OP_RESCUE\tR%d\tR%d", a, b);
      print_lv_ab(mrb, irep, a, b);
      break;
//    CASE(OP_RAISEIF, B);
//      printf("OP_RAISEIF\tR%d\t\t", a);
//      print_lv_a(mrb, irep, a);
//      break;

    CASE(OP_DEBUG, BBB);
      printf("OP_DEBUG\t%d\t%d\t%d\n", a, b, c);
      break;

    CASE(OP_STOP, Z);
      printf("OP_STOP\n");
      break;

    default:
      printf("OP_unknown (0x%x)\n", ins);
      break;
    }
    printf("\n");
  }
}
