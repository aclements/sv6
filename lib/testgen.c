
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include "fstest.h"

static int xerrno(int r) {
#ifdef XV6_USER
  return r;
#else
  return -errno;
#endif
}

    static void setup_0(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_0_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_0_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_1(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_1_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_1_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_2(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_2_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_2_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_3(void) {
      
    }
    
      static int test_3_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_3_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_4(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_4_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_4_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_5(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_5_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_5_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_6(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_6_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_6_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_7(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_7_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_7_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_8(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_8_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_8_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_9(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_9_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_9_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_10(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_10_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_10_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_11(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_11_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_11_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_12(void) {
      
    }
    
      static int test_12_0(void) {
        
      {
        int fd = open("1", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_12_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_13(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_13_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_13_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_14(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_14_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_14_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_15(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_15_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_15_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_16(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_16_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_16_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_17(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_17_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_17_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_18(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_18_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_18_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_19(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_19_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_19_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_20(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_20_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_20_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_21(void) {
      
    }
    
      static int test_21_0(void) {
        
      {
        int fd = open("1", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_21_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_22(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_22_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_22_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_23(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_23_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_23_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_24(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_24_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_24_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_25(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_25_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_25_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_26(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_26_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_26_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_27(void) {
      
    }
    
      static int test_27_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_27_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_28(void) {
      
    }
    
      static int test_28_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_28_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_29(void) {
      
    }
    
      static int test_29_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_29_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_30(void) {
      
    }
    
      static int test_30_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_30_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_31(void) {
      
    }
    
      static int test_31_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_31_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_32(void) {
      
    }
    
      static int test_32_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_32_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_33(void) {
      
    }
    
      static int test_33_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_33_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_34(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_34_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_34_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_35(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_35_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_35_1(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_36(void) {
      
    }
    
      static int test_36_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_36_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_37(void) {
      
    }
    
      static int test_37_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_37_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_38(void) {
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i10", "3");
      
      unlink("__i10");
      
    }
    
      static int test_38_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_38_1(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_39(void) {
      
    }
    
      static int test_39_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_39_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_40(void) {
      
    }
    
      static int test_40_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_40_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_41(void) {
      
    }
    
      static int test_41_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_41_1(void) {
        
      {
        int fd = open("4", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_42(void) {
      
    }
    
      static int test_42_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_42_1(void) {
        
      {
        int fd = open("4", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_43(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_43_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_43_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_44(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_44_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_44_1(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_45(void) {
      
    }
    
      static int test_45_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_45_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_46(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_46_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_46_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_47(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_47_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_47_1(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_48(void) {
      
    }
    
      static int test_48_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_48_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_49(void) {
      
    }
    
      static int test_49_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_49_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_50(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_50_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_50_1(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_51(void) {
      
    }
    
      static int test_51_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_51_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_52(void) {
      
    }
    
      static int test_52_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_52_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_53(void) {
      
    }
    
      static int test_53_0(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_53_1(void) {
        
      {
        int fd = open("4", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_54(void) {
      
    }
    
      static int test_54_0(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_54_1(void) {
        
      {
        int fd = open("4", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_55(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_55_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_55_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_56(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_56_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_56_1(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_57(void) {
      
    }
    
      static int test_57_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_57_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_58(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_58_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_58_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_59(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_59_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_59_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_60(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_60_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_60_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_61(void) {
      
    }
    
      static int test_61_0(void) {
        
      {
        int fd = open("1", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_61_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_62(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_62_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_62_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_63(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_63_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_63_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_64(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_64_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_64_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_65(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_65_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_65_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_66(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "2");
      
      unlink("__i11");
      
    }
    
      static int test_66_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_66_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_67(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_67_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_67_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_68(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_68_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_68_1(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_69(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_69_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_69_1(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_70(void) {
      
    }
    
      static int test_70_0(void) {
        
      {
        int fd = open("1", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_70_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_71(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_71_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_71_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_72(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "2");
      
      unlink("__i8");
      
    }
    
      static int test_72_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_72_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_73(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_73_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_73_1(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_74(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_74_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_74_1(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_75(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "2");
      
      unlink("__i7");
      
    }
    
      static int test_75_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_75_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_76(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_76_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_76_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_77(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_77_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_77_1(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_78(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      unlink("__i7");
      
    }
    
      static int test_78_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_78_1(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_79(void) {
      
    }
    
      static int test_79_0(void) {
        
      {
        int fd = open("1", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_79_1(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_80(void) {
      
    }
    
      static int test_80_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_80_1(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_81(void) {
      
    }
    
      static int test_81_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_81_1(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_82(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_82_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_82_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_83(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      unlink("__i11");
      
    }
    
      static int test_83_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_83_1(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_84(void) {
      
    }
    
      static int test_84_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_84_1(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
    static void setup_85(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i4", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i5", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_85_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_85_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_86(void) {
      
    }
    
      static int test_86_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_86_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_87(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i4", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i5", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_87_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_87_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_88(void) {
      
    }
    
      static int test_88_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_88_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_89(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i4", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i5", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_89_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_89_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_90(void) {
      
    }
    
      static int test_90_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_90_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_91(void) {
      
    }
    
      static int test_91_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_91_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_92(void) {
      
    }
    
      static int test_92_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_92_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_93(void) {
      
    }
    
      static int test_93_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_93_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_94(void) {
      
    }
    
      static int test_94_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_94_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_95(void) {
      
    }
    
      static int test_95_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_95_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_96(void) {
      
    }
    
      static int test_96_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_96_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_97(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i4", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i5", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_97_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_97_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_98(void) {
      
    }
    
      static int test_98_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_98_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_99(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i4", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i5", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_99_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_99_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_100(void) {
      
    }
    
      static int test_100_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_100_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_101(void) {
      
    }
    
      static int test_101_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_101_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_102(void) {
      
    }
    
      static int test_102_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_102_1(void) {
        
      {
        int fd = open("3", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_103(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_103_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_103_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_104(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_104_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_104_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_105(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_105_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_105_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_106(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_106_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_106_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_107(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_107_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_107_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_108(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_108_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_108_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_109(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_109_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_109_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_110(void) {
      
    }
    
      static int test_110_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_110_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_111(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_111_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_111_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_112(void) {
      
    }
    
      static int test_112_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_112_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_113(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_113_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_113_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_114(void) {
      
    }
    
      static int test_114_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_114_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_115(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_115_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_115_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_116(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_116_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_116_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_117(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_117_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_117_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_118(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_118_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_118_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_119(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_119_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_119_1(void) {
        
      {
        int fd = open("3", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_120(void) {
      
    }
    
      static int test_120_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_120_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_121(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i7", "6");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_121_0(void) {
        
      {
        int fd = open("6", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_121_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_122(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_122_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_122_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_123(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_123_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_123_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_124(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i7", "6");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_124_0(void) {
        
      {
        int fd = open("6", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_124_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_125(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_125_0(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_125_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_126(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_126_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_126_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_127(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i7", "6");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_127_0(void) {
        
      {
        int fd = open("6", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_127_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_128(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_128_0(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_128_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_129(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_129_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_129_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_130(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_130_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_130_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_131(void) {
      
    }
    
      static int test_131_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_131_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_132(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i7", "4");
      
      unlink("__i7");
      
    }
    
      static int test_132_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_132_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_133(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      unlink("__i5");
      
    }
    
      static int test_133_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_133_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_134(void) {
      
    }
    
      static int test_134_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_134_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_135(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i7", "4");
      
      unlink("__i7");
      
    }
    
      static int test_135_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_135_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_136(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_136_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_136_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_137(void) {
      
    }
    
      static int test_137_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_137_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_138(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i7", "6");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_138_0(void) {
        
      {
        int fd = open("6", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_138_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_139(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_139_0(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_139_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_140(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_140_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_140_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_141(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i7", "6");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_141_0(void) {
        
      {
        int fd = open("6", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_141_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_142(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_142_0(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_142_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_143(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_143_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_143_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_144(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i6", "4");
      
      unlink("__i6");
      
    }
    
      static int test_144_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_144_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_145(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      unlink("__i5");
      
    }
    
      static int test_145_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_145_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_146(void) {
      
    }
    
      static int test_146_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_146_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_147(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i4", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_147_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_147_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_148(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_148_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_148_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_149(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_149_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_149_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_150(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i4", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_150_0(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_150_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_151(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i7", "2");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_151_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_151_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_152(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_152_0(void) {
        
      {
        int fd = open("3", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_152_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_153(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i4", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_153_0(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_153_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_154(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_154_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_154_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_155(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_155_0(void) {
        
      {
        int fd = open("3", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_155_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_156(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i5", "2");
      
      unlink("__i8");
      
      unlink("__i5");
      
    }
    
      static int test_156_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_156_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_157(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_157_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_157_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_158(void) {
      
    }
    
      static int test_158_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_158_1(void) {
        
      return link("1", "2");
      
      }
      
    static void setup_159(void) {
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i10", "4");
      
      unlink("__i10");
      
      unlink("__i7");
      
    }
    
      static int test_159_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_159_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_160(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_160_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_160_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_161(void) {
      
    }
    
      static int test_161_0(void) {
        
      {
        int fd = open("3", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_161_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_162(void) {
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i10", "4");
      
      unlink("__i10");
      
      unlink("__i7");
      
    }
    
      static int test_162_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_162_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_163(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_163_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_163_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_164(void) {
      
    }
    
      static int test_164_0(void) {
        
      {
        int fd = open("3", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_164_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_165(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i4", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_165_0(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_165_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_166(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_166_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_166_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_167(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_167_0(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_167_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_168(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i4", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_168_0(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_168_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_169(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i7", "2");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_169_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_169_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_170(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_170_0(void) {
        
      {
        int fd = open("3", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_170_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_171(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i9", "4");
      
      unlink("__i9");
      
      unlink("__i7");
      
    }
    
      static int test_171_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_171_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_172(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_172_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_172_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_173(void) {
      
    }
    
      static int test_173_0(void) {
        
      {
        int fd = open("3", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_173_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_174(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      link("__i6", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_174_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_174_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_175(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_175_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_175_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_176(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_176_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_176_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_177(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_177_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_177_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_178(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      link("__i6", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_178_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_178_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_179(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i5", "2");
      
      unlink("__i8");
      
      unlink("__i5");
      
    }
    
      static int test_179_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_179_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_180(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_180_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_180_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_181(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_181_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_181_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_182(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      link("__i6", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_182_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_182_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_183(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_183_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_183_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_184(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_184_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_184_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_185(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "2");
      
      unlink("__i7");
      
    }
    
      static int test_185_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_185_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_186(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_186_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_186_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_187(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_187_0(void) {
        
      {
        int fd = open("1", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_187_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_188(void) {
      
    }
    
      static int test_188_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_188_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_189(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 10;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 10;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      link("__i8", "5");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i11");
      
    }
    
      static int test_189_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_189_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_190(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i10", "3");
      
      link("__i9", "4");
      
      unlink("__i9");
      
      unlink("__i10");
      
    }
    
      static int test_190_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_190_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_191(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_191_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_191_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_192(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_192_0(void) {
        
      {
        int fd = open("2", 0x242, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_192_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_193(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_193_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_193_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_194(void) {
      
    }
    
      static int test_194_0(void) {
        
      {
        int fd = open("2", 0x2c2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_194_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_195(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 10;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 10;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      link("__i8", "5");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i11");
      
    }
    
      static int test_195_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_195_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_196(void) {
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i10", "4");
      
      unlink("__i10");
      
      unlink("__i6");
      
    }
    
      static int test_196_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_196_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_197(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      unlink("__i8");
      
    }
    
      static int test_197_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_197_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_198(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_198_0(void) {
        
      {
        int fd = open("2", 0x42, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_198_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_199(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_199_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_199_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_200(void) {
      
    }
    
      static int test_200_0(void) {
        
      {
        int fd = open("2", 0xc2, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_200_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_201(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      link("__i6", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_201_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_201_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_202(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_202_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_202_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_203(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_203_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_203_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_204(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_204_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_204_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_205(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      link("__i6", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_205_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_205_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_206(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i7");
      
    }
    
      static int test_206_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_206_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_207(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i6", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_207_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_207_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_208(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_208_0(void) {
        
      {
        int fd = open("2", 0x82, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_208_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_209(void) {
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "3");
      
      link("__i7", "5");
      
      link("__i7", "4");
      
      unlink("__i11");
      
      unlink("__i7");
      
    }
    
      static int test_209_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_209_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_210(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_210_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_210_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_211(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      unlink("__i9");
      
    }
    
      static int test_211_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_211_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_212(void) {
      
    }
    
      static int test_212_0(void) {
        
      {
        int fd = open("2", 0x282, 0666);
        if (fd < 0)
          return xerrno(fd);
        close(fd);
        return 0;
      }
      
      }
      
      static int test_212_1(void) {
        
      return rename("2", "2");
      
      }
      
    static void setup_213(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "11");
      
      link("__i3", "10");
      
      link("__i3", "13");
      
      link("__i3", "12");
      
      link("__i3", "14");
      
      link("__i3", "1");
      
      link("__i3", "0");
      
      link("__i3", "3");
      
      link("__i4", "2");
      
      link("__i3", "5");
      
      link("__i3", "4");
      
      link("__i3", "7");
      
      link("__i3", "6");
      
      link("__i3", "9");
      
      link("__i3", "8");
      
      unlink("__i3");
      
      unlink("__i4");
      
    }
    
      static int test_213_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_213_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_214(void) {
      
    }
    
      static int test_214_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_214_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_215(void) {
      
    }
    
      static int test_215_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_215_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_216(void) {
      
    }
    
      static int test_216_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_216_1(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
    static void setup_217(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 4;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 4;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "11");
      
      link("__i3", "10");
      
      link("__i3", "13");
      
      link("__i3", "12");
      
      link("__i3", "14");
      
      link("__i3", "1");
      
      link("__i3", "0");
      
      link("__i3", "3");
      
      link("__i5", "2");
      
      link("__i3", "5");
      
      link("__i3", "4");
      
      link("__i3", "7");
      
      link("__i3", "6");
      
      link("__i3", "9");
      
      link("__i3", "8");
      
      unlink("__i3");
      
      unlink("__i5");
      
    }
    
      static int test_217_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_217_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_218(void) {
      
    }
    
      static int test_218_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_218_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_219(void) {
      
    }
    
      static int test_219_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_219_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_220(void) {
      
    }
    
      static int test_220_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_220_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 0;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_221(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "11");
      
      link("__i3", "10");
      
      link("__i3", "13");
      
      link("__i3", "12");
      
      link("__i3", "14");
      
      link("__i3", "1");
      
      link("__i3", "0");
      
      link("__i3", "3");
      
      link("__i3", "2");
      
      link("__i3", "5");
      
      link("__i5", "4");
      
      link("__i3", "7");
      
      link("__i3", "6");
      
      link("__i3", "9");
      
      link("__i3", "8");
      
      unlink("__i3");
      
      unlink("__i5");
      
    }
    
      static int test_221_0(void) {
        
      {
        int fd = open("4", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_221_1(void) {
        
      return unlink("1");
      
      }
      
    static void setup_222(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i3", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i4", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i3");
      
      unlink("__i4");
      
    }
    
      static int test_222_0(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_222_1(void) {
        
      return unlink("1");
      
      }
      
    static void setup_223(void) {
      
    }
    
      static int test_223_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_223_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_224(void) {
      
    }
    
      static int test_224_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_224_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_225(void) {
      
    }
    
      static int test_225_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_225_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_226(void) {
      
    }
    
      static int test_226_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_226_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_227(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "11");
      
      link("__i6", "10");
      
      link("__i6", "13");
      
      link("__i6", "12");
      
      link("__i6", "14");
      
      link("__i4", "1");
      
      link("__i6", "0");
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      link("__i6", "5");
      
      link("__i6", "4");
      
      link("__i6", "7");
      
      link("__i6", "6");
      
      link("__i6", "9");
      
      link("__i6", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_227_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_227_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_228(void) {
      
    }
    
      static int test_228_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_228_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_229(void) {
      
    }
    
      static int test_229_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_229_1(void) {
        
      return link("2", "0");
      
      }
      
    static void setup_230(void) {
      
    }
    
      static int test_230_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_230_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_231(void) {
      
    }
    
      static int test_231_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_231_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_232(void) {
      
    }
    
      static int test_232_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_232_1(void) {
        
      return link("2", "0");
      
      }
      
    static void setup_233(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "11");
      
      link("__i5", "10");
      
      link("__i5", "13");
      
      link("__i5", "12");
      
      link("__i5", "14");
      
      link("__i4", "1");
      
      link("__i5", "0");
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i5", "5");
      
      link("__i5", "4");
      
      link("__i5", "7");
      
      link("__i5", "6");
      
      link("__i5", "9");
      
      link("__i5", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_233_0(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_233_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_234(void) {
      
    }
    
      static int test_234_0(void) {
        
      {
        int fd = open("2", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_234_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_235(void) {
      
    }
    
      static int test_235_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_235_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_236(void) {
      
    }
    
      static int test_236_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_236_1(void) {
        
      return rename("2", "0");
      
      }
      
    static void setup_237(void) {
      
    }
    
      static int test_237_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_237_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_238(void) {
      
    }
    
      static int test_238_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_238_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_239(void) {
      
    }
    
      static int test_239_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_239_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_240(void) {
      
    }
    
      static int test_240_0(void) {
        
      {
        int fd = open("1", O_RDONLY);
        if (fd < 0)
          return xerrno(fd);
        char c;
        ssize_t cc = read(fd, &c, 1);
        close(fd);
        return (cc > 0) ? c : INT_MIN;
      }
      
      }
      
      static int test_240_1(void) {
        
      return rename("2", "0");
      
      }
      
    static void setup_241(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i5", "2");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_241_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_241_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_242(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      unlink("__i4");
      
    }
    
      static int test_242_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_242_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_243(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_243_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_243_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_244(void) {
      
    }
    
      static int test_244_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_244_1(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
    static void setup_245(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i4", "2");
      
      link("__i6", "5");
      
      unlink("__i4");
      
      unlink("__i6");
      
    }
    
      static int test_245_0(void) {
        
      {
        int fd = open("5", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_245_1(void) {
        
      return unlink("1");
      
      }
      
    static void setup_246(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i5", "2");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_246_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_246_1(void) {
        
      return unlink("1");
      
      }
      
    static void setup_247(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      unlink("__i4");
      
    }
    
      static int test_247_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_247_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_248(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_248_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_248_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_249(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_249_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_249_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_250(void) {
      
    }
    
      static int test_250_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_250_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_251(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 5;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "1");
      
      link("__i7", "2");
      
      unlink("__i3");
      
      unlink("__i7");
      
    }
    
      static int test_251_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_251_1(void) {
        
      return link("1", "2");
      
      }
      
    static void setup_252(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_252_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_252_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_253(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_253_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_253_1(void) {
        
      return link("1", "2");
      
      }
      
    static void setup_254(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "3");
      
      link("__i5", "2");
      
      unlink("__i9");
      
      unlink("__i5");
      
    }
    
      static int test_254_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_254_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_255(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_255_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_255_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_256(void) {
      
    }
    
      static int test_256_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_256_1(void) {
        
      return link("1", "2");
      
      }
      
    static void setup_257(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "1");
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_257_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 10;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_257_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_258(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i7", "2");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_258_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 6;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_258_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_259(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_259_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_259_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_260(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 6;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_260_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_260_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_261(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 9;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_261_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 10;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_261_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_262(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 8;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_262_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 9;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_262_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_263(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 7;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_263_0(void) {
        
      {
        int fd = open("1", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 8;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_263_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_264(void) {
      
    }
    
      static int test_264_0(void) {
        
      {
        int fd = open("2", O_WRONLY | O_TRUNC);
        if (fd < 0)
          return xerrno(fd);
        char c = 7;
        ssize_t cc = write(fd, &c, 1);
        close(fd);
        return cc;
      }
      
      }
      
      static int test_264_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_265(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "11");
      
      link("__i3", "10");
      
      link("__i3", "13");
      
      link("__i3", "12");
      
      link("__i3", "14");
      
      link("__i3", "1");
      
      link("__i3", "0");
      
      link("__i3", "3");
      
      link("__i3", "2");
      
      link("__i6", "5");
      
      link("__i6", "4");
      
      link("__i3", "7");
      
      link("__i3", "6");
      
      link("__i3", "9");
      
      link("__i3", "8");
      
      unlink("__i3");
      
      unlink("__i6");
      
    }
    
      static int test_265_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_265_1(void) {
        
      return unlink("4");
      
      }
      
    static void setup_266(void) {
      
    }
    
      static int test_266_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_266_1(void) {
        
      return unlink("3");
      
      }
      
    static void setup_267(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i3", "11");
      
      link("__i3", "10");
      
      link("__i3", "13");
      
      link("__i3", "12");
      
      link("__i3", "14");
      
      link("__i3", "1");
      
      link("__i3", "0");
      
      link("__i3", "3");
      
      link("__i3", "2");
      
      link("__i3", "5");
      
      link("__i5", "4");
      
      link("__i3", "7");
      
      link("__i3", "6");
      
      link("__i3", "9");
      
      link("__i3", "8");
      
      unlink("__i3");
      
      unlink("__i5");
      
    }
    
      static int test_267_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_267_1(void) {
        
      return unlink("4");
      
      }
      
    static void setup_268(void) {
      
    }
    
      static int test_268_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_268_1(void) {
        
      return unlink("4");
      
      }
      
    static void setup_269(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "11");
      
      link("__i4", "10");
      
      link("__i4", "13");
      
      link("__i4", "12");
      
      link("__i4", "14");
      
      link("__i5", "1");
      
      link("__i4", "0");
      
      link("__i4", "3");
      
      link("__i4", "2");
      
      link("__i4", "5");
      
      link("__i4", "4");
      
      link("__i4", "7");
      
      link("__i4", "6");
      
      link("__i4", "9");
      
      link("__i4", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
    }
    
      static int test_269_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_269_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_270(void) {
      
    }
    
      static int test_270_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_270_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_271(void) {
      
    }
    
      static int test_271_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_271_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_272(void) {
      
    }
    
      static int test_272_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_272_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_273(void) {
      
    }
    
      static int test_273_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_273_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_274(void) {
      
    }
    
      static int test_274_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_274_1(void) {
        
      return unlink("2");
      
      }
      
    static void setup_275(void) {
      
      {
        int fd = open("__i3", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "11");
      
      link("__i7", "10");
      
      link("__i7", "13");
      
      link("__i7", "12");
      
      link("__i7", "14");
      
      link("__i3", "1");
      
      link("__i7", "0");
      
      link("__i7", "3");
      
      link("__i3", "2");
      
      link("__i7", "5");
      
      link("__i6", "4");
      
      link("__i7", "7");
      
      link("__i7", "6");
      
      link("__i7", "9");
      
      link("__i7", "8");
      
      unlink("__i3");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_275_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_275_1(void) {
        
      return link("4", "5");
      
      }
      
    static void setup_276(void) {
      
    }
    
      static int test_276_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_276_1(void) {
        
      return link("4", "5");
      
      }
      
    static void setup_277(void) {
      
    }
    
      static int test_277_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_277_1(void) {
        
      return link("4", "0");
      
      }
      
    static void setup_278(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "11");
      
      link("__i6", "10");
      
      link("__i6", "13");
      
      link("__i6", "12");
      
      link("__i6", "14");
      
      link("__i4", "1");
      
      link("__i6", "0");
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      link("__i6", "5");
      
      link("__i6", "4");
      
      link("__i6", "7");
      
      link("__i6", "6");
      
      link("__i6", "9");
      
      link("__i6", "8");
      
      unlink("__i4");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_278_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_278_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_279(void) {
      
    }
    
      static int test_279_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_279_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_280(void) {
      
    }
    
      static int test_280_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_280_1(void) {
        
      return link("2", "0");
      
      }
      
    static void setup_281(void) {
      
    }
    
      static int test_281_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_281_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_282(void) {
      
    }
    
      static int test_282_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_282_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_283(void) {
      
    }
    
      static int test_283_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_283_1(void) {
        
      return link("2", "0");
      
      }
      
    static void setup_284(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i4", "2");
      
      link("__i8", "5");
      
      link("__i9", "7");
      
      link("__i9", "6");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i4");
      
    }
    
      static int test_284_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_284_1(void) {
        
      return rename("5", "6");
      
      }
      
    static void setup_285(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i5", "3");
      
      link("__i6", "4");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_285_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_285_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_286(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i4", "2");
      
      link("__i7", "5");
      
      link("__i8", "6");
      
      unlink("__i8");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_286_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_286_1(void) {
        
      return rename("5", "6");
      
      }
      
    static void setup_287(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i4", "2");
      
      link("__i7", "5");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_287_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_287_1(void) {
        
      return rename("5", "6");
      
      }
      
    static void setup_288(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      link("__i4", "2");
      
      unlink("__i4");
      
    }
    
      static int test_288_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_288_1(void) {
        
      return rename("5", "6");
      
      }
      
    static void setup_289(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "1");
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_289_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_289_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_290(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_290_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_290_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_291(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i6", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_291_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_291_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_292(void) {
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i4", "1");
      
      unlink("__i4");
      
    }
    
      static int test_292_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_292_1(void) {
        
      return rename("2", "5");
      
      }
      
    static void setup_293(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_293_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_293_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_294(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      link("__i5", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_294_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_294_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_295(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "2");
      
      unlink("__i5");
      
    }
    
      static int test_295_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_295_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_296(void) {
      
    }
    
      static int test_296_0(void) {
        
      return unlink("1");
      
      }
      
      static int test_296_1(void) {
        
      return rename("2", "4");
      
      }
      
    static void setup_297(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i7", "3");
      
      link("__i8", "2");
      
      link("__i9", "4");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_297_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_297_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_298(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i7", "3");
      
      link("__i8", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_298_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_298_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_299(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i8", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
    }
    
      static int test_299_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_299_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_300(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i7", "3");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_300_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_300_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_301(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i7", "3");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_301_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_301_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_302(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      unlink("__i6");
      
    }
    
      static int test_302_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_302_1(void) {
        
      return link("3", "4");
      
      }
      
    static void setup_303(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "3");
      
      link("__i7", "2");
      
      unlink("__i8");
      
      unlink("__i7");
      
    }
    
      static int test_303_0(void) {
        
      return link("1", "4");
      
      }
      
      static int test_303_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_304(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "2");
      
      unlink("__i7");
      
    }
    
      static int test_304_0(void) {
        
      return link("1", "4");
      
      }
      
      static int test_304_1(void) {
        
      return link("2", "3");
      
      }
      
    static void setup_305(void) {
      
    }
    
      static int test_305_0(void) {
        
      return link("1", "3");
      
      }
      
      static int test_305_1(void) {
        
      return link("2", "4");
      
      }
      
    static void setup_306(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "1");
      
      link("__i8", "3");
      
      link("__i10", "2");
      
      link("__i7", "5");
      
      link("__i7", "4");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i10");
      
      unlink("__i7");
      
    }
    
      static int test_306_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_306_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_307(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i6", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_307_0(void) {
        
      return link("2", "2");
      
      }
      
      static int test_307_1(void) {
        
      return rename("1", "2");
      
      }
      
    static void setup_308(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      link("__i8", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_308_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_308_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_309(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_309_0(void) {
        
      return link("3", "1");
      
      }
      
      static int test_309_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_310(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i10", "1");
      
      link("__i8", "9");
      
      link("__i6", "3");
      
      link("__i8", "5");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i10");
      
      unlink("__i6");
      
    }
    
      static int test_310_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_310_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_311(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_311_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_311_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_312(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_312_0(void) {
        
      return link("1", "2");
      
      }
      
      static int test_312_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_313(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "3");
      
      unlink("__i6");
      
    }
    
      static int test_313_0(void) {
        
      return link("3", "1");
      
      }
      
      static int test_313_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_314(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_314_0(void) {
        
      return link("1", "8");
      
      }
      
      static int test_314_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_315(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_315_0(void) {
        
      return link("1", "4");
      
      }
      
      static int test_315_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_316(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_316_0(void) {
        
      return link("1", "4");
      
      }
      
      static int test_316_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_317(void) {
      
    }
    
      static int test_317_0(void) {
        
      return link("3", "2");
      
      }
      
      static int test_317_1(void) {
        
      return rename("1", "3");
      
      }
      
    static void setup_318(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i9", "8");
      
      link("__i6", "3");
      
      link("__i6", "2");
      
      link("__i9", "7");
      
      unlink("__i9");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_318_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_318_1(void) {
        
      return rename("3", "7");
      
      }
      
    static void setup_319(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i4", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "1");
      
      link("__i4", "2");
      
      link("__i7", "5");
      
      link("__i4", "6");
      
      unlink("__i8");
      
      unlink("__i4");
      
      unlink("__i7");
      
    }
    
      static int test_319_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_319_1(void) {
        
      return rename("5", "6");
      
      }
      
    static void setup_320(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i10", "1");
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i9", "7");
      
      link("__i8", "6");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i10");
      
      unlink("__i5");
      
    }
    
      static int test_320_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_320_1(void) {
        
      return rename("6", "7");
      
      }
      
    static void setup_321(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "1");
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      link("__i8", "6");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i5");
      
    }
    
      static int test_321_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_321_1(void) {
        
      return rename("6", "7");
      
      }
      
    static void setup_322(void) {
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "1");
      
      link("__i5", "3");
      
      link("__i5", "2");
      
      unlink("__i9");
      
      unlink("__i5");
      
    }
    
      static int test_322_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_322_1(void) {
        
      return rename("6", "7");
      
      }
      
    static void setup_323(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i10", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i11", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i11", "1");
      
      link("__i7", "3");
      
      link("__i10", "2");
      
      link("__i8", "5");
      
      link("__i8", "4");
      
      link("__i8", "9");
      
      unlink("__i8");
      
      unlink("__i10");
      
      unlink("__i11");
      
      unlink("__i7");
      
    }
    
      static int test_323_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_323_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_324(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i7", "3");
      
      link("__i8", "2");
      
      link("__i7", "4");
      
      link("__i7", "6");
      
      unlink("__i8");
      
      unlink("__i7");
      
    }
    
      static int test_324_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_324_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_325(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      link("__i8", "2");
      
      link("__i9", "4");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_325_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_325_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_326(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "1");
      
      link("__i8", "3");
      
      link("__i7", "2");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_326_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_326_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_327(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i6", "2");
      
      unlink("__i5");
      
      unlink("__i6");
      
    }
    
      static int test_327_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_327_1(void) {
        
      return rename("3", "7");
      
      }
      
    static void setup_328(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i9", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i9", "1");
      
      link("__i6", "3");
      
      link("__i8", "5");
      
      link("__i8", "4");
      
      unlink("__i8");
      
      unlink("__i9");
      
      unlink("__i6");
      
    }
    
      static int test_328_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_328_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_329(void) {
      
      {
        int fd = open("__i5", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i5", "1");
      
      link("__i5", "3");
      
      unlink("__i5");
      
    }
    
      static int test_329_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_329_1(void) {
        
      return rename("3", "2");
      
      }
      
    static void setup_330(void) {
      
      {
        int fd = open("__i8", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i8", "1");
      
      link("__i6", "3");
      
      link("__i7", "4");
      
      unlink("__i8");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_330_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_330_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_331(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      link("__i6", "3");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_331_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_331_1(void) {
        
      return rename("3", "4");
      
      }
      
    static void setup_332(void) {
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "1");
      
      unlink("__i7");
      
    }
    
      static int test_332_0(void) {
        
      return rename("1", "2");
      
      }
      
      static int test_332_1(void) {
        
      return rename("3", "5");
      
      }
      
    static void setup_333(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      link("__i7", "4");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_333_0(void) {
        
      return rename("1", "8");
      
      }
      
      static int test_333_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_334(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      {
        int fd = open("__i7", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i7", "3");
      
      link("__i6", "2");
      
      unlink("__i6");
      
      unlink("__i7");
      
    }
    
      static int test_334_0(void) {
        
      return rename("1", "4");
      
      }
      
      static int test_334_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_335(void) {
      
      {
        int fd = open("__i6", O_CREAT | O_EXCL | O_RDWR, 0666);
        char c = 0;
        write(fd, &c, 1);
        close(fd);
      }
      
      link("__i6", "2");
      
      unlink("__i6");
      
    }
    
      static int test_335_0(void) {
        
      return rename("1", "4");
      
      }
      
      static int test_335_1(void) {
        
      return rename("2", "3");
      
      }
      
    static void setup_336(void) {
      
    }
    
      static int test_336_0(void) {
        
      return rename("5", "2");
      
      }
      
      static int test_336_1(void) {
        
      return rename("1", "5");
      
      }
      
  static void cleanup(void) {
    
      unlink("0");
      
      unlink("1");
      
      unlink("2");
      
      unlink("3");
      
      unlink("4");
      
      unlink("5");
      
      unlink("6");
      
      unlink("7");
      
      unlink("8");
      
      unlink("9");
      
      unlink("10");
      
      unlink("11");
      
      unlink("12");
      
      unlink("13");
      
      unlink("14");
      
  }
  
  struct fstest fstests[] = {
  
    { &setup_0, &test_0_0, &test_0_1, "open", "open", &cleanup },
    
    { &setup_1, &test_1_0, &test_1_1, "open", "open", &cleanup },
    
    { &setup_2, &test_2_0, &test_2_1, "open", "open", &cleanup },
    
    { &setup_3, &test_3_0, &test_3_1, "open", "open", &cleanup },
    
    { &setup_4, &test_4_0, &test_4_1, "open", "open", &cleanup },
    
    { &setup_5, &test_5_0, &test_5_1, "open", "open", &cleanup },
    
    { &setup_6, &test_6_0, &test_6_1, "open", "open", &cleanup },
    
    { &setup_7, &test_7_0, &test_7_1, "open", "open", &cleanup },
    
    { &setup_8, &test_8_0, &test_8_1, "open", "open", &cleanup },
    
    { &setup_9, &test_9_0, &test_9_1, "open", "open", &cleanup },
    
    { &setup_10, &test_10_0, &test_10_1, "open", "open", &cleanup },
    
    { &setup_11, &test_11_0, &test_11_1, "open", "open", &cleanup },
    
    { &setup_12, &test_12_0, &test_12_1, "open", "open", &cleanup },
    
    { &setup_13, &test_13_0, &test_13_1, "open", "open", &cleanup },
    
    { &setup_14, &test_14_0, &test_14_1, "open", "open", &cleanup },
    
    { &setup_15, &test_15_0, &test_15_1, "open", "open", &cleanup },
    
    { &setup_16, &test_16_0, &test_16_1, "open", "open", &cleanup },
    
    { &setup_17, &test_17_0, &test_17_1, "open", "open", &cleanup },
    
    { &setup_18, &test_18_0, &test_18_1, "open", "open", &cleanup },
    
    { &setup_19, &test_19_0, &test_19_1, "open", "open", &cleanup },
    
    { &setup_20, &test_20_0, &test_20_1, "open", "open", &cleanup },
    
    { &setup_21, &test_21_0, &test_21_1, "open", "open", &cleanup },
    
    { &setup_22, &test_22_0, &test_22_1, "open", "open", &cleanup },
    
    { &setup_23, &test_23_0, &test_23_1, "open", "open", &cleanup },
    
    { &setup_24, &test_24_0, &test_24_1, "open", "open", &cleanup },
    
    { &setup_25, &test_25_0, &test_25_1, "open", "open", &cleanup },
    
    { &setup_26, &test_26_0, &test_26_1, "open", "open", &cleanup },
    
    { &setup_27, &test_27_0, &test_27_1, "open", "open", &cleanup },
    
    { &setup_28, &test_28_0, &test_28_1, "open", "open", &cleanup },
    
    { &setup_29, &test_29_0, &test_29_1, "open", "open", &cleanup },
    
    { &setup_30, &test_30_0, &test_30_1, "open", "open", &cleanup },
    
    { &setup_31, &test_31_0, &test_31_1, "open", "open", &cleanup },
    
    { &setup_32, &test_32_0, &test_32_1, "open", "open", &cleanup },
    
    { &setup_33, &test_33_0, &test_33_1, "open", "open", &cleanup },
    
    { &setup_34, &test_34_0, &test_34_1, "open", "open", &cleanup },
    
    { &setup_35, &test_35_0, &test_35_1, "open", "open", &cleanup },
    
    { &setup_36, &test_36_0, &test_36_1, "open", "open", &cleanup },
    
    { &setup_37, &test_37_0, &test_37_1, "open", "open", &cleanup },
    
    { &setup_38, &test_38_0, &test_38_1, "open", "open", &cleanup },
    
    { &setup_39, &test_39_0, &test_39_1, "open", "open", &cleanup },
    
    { &setup_40, &test_40_0, &test_40_1, "open", "open", &cleanup },
    
    { &setup_41, &test_41_0, &test_41_1, "open", "open", &cleanup },
    
    { &setup_42, &test_42_0, &test_42_1, "open", "open", &cleanup },
    
    { &setup_43, &test_43_0, &test_43_1, "open", "open", &cleanup },
    
    { &setup_44, &test_44_0, &test_44_1, "open", "open", &cleanup },
    
    { &setup_45, &test_45_0, &test_45_1, "open", "open", &cleanup },
    
    { &setup_46, &test_46_0, &test_46_1, "open", "open", &cleanup },
    
    { &setup_47, &test_47_0, &test_47_1, "open", "open", &cleanup },
    
    { &setup_48, &test_48_0, &test_48_1, "open", "open", &cleanup },
    
    { &setup_49, &test_49_0, &test_49_1, "open", "open", &cleanup },
    
    { &setup_50, &test_50_0, &test_50_1, "open", "open", &cleanup },
    
    { &setup_51, &test_51_0, &test_51_1, "open", "open", &cleanup },
    
    { &setup_52, &test_52_0, &test_52_1, "open", "open", &cleanup },
    
    { &setup_53, &test_53_0, &test_53_1, "open", "open", &cleanup },
    
    { &setup_54, &test_54_0, &test_54_1, "open", "open", &cleanup },
    
    { &setup_55, &test_55_0, &test_55_1, "open", "open", &cleanup },
    
    { &setup_56, &test_56_0, &test_56_1, "open", "open", &cleanup },
    
    { &setup_57, &test_57_0, &test_57_1, "open", "open", &cleanup },
    
    { &setup_58, &test_58_0, &test_58_1, "open", "open", &cleanup },
    
    { &setup_59, &test_59_0, &test_59_1, "open", "open", &cleanup },
    
    { &setup_60, &test_60_0, &test_60_1, "open", "open", &cleanup },
    
    { &setup_61, &test_61_0, &test_61_1, "open", "open", &cleanup },
    
    { &setup_62, &test_62_0, &test_62_1, "open", "open", &cleanup },
    
    { &setup_63, &test_63_0, &test_63_1, "open", "open", &cleanup },
    
    { &setup_64, &test_64_0, &test_64_1, "open", "open", &cleanup },
    
    { &setup_65, &test_65_0, &test_65_1, "open", "open", &cleanup },
    
    { &setup_66, &test_66_0, &test_66_1, "open", "open", &cleanup },
    
    { &setup_67, &test_67_0, &test_67_1, "open", "open", &cleanup },
    
    { &setup_68, &test_68_0, &test_68_1, "open", "open", &cleanup },
    
    { &setup_69, &test_69_0, &test_69_1, "open", "open", &cleanup },
    
    { &setup_70, &test_70_0, &test_70_1, "open", "open", &cleanup },
    
    { &setup_71, &test_71_0, &test_71_1, "open", "open", &cleanup },
    
    { &setup_72, &test_72_0, &test_72_1, "open", "open", &cleanup },
    
    { &setup_73, &test_73_0, &test_73_1, "open", "open", &cleanup },
    
    { &setup_74, &test_74_0, &test_74_1, "open", "open", &cleanup },
    
    { &setup_75, &test_75_0, &test_75_1, "open", "open", &cleanup },
    
    { &setup_76, &test_76_0, &test_76_1, "open", "open", &cleanup },
    
    { &setup_77, &test_77_0, &test_77_1, "open", "open", &cleanup },
    
    { &setup_78, &test_78_0, &test_78_1, "open", "open", &cleanup },
    
    { &setup_79, &test_79_0, &test_79_1, "open", "open", &cleanup },
    
    { &setup_80, &test_80_0, &test_80_1, "open", "open", &cleanup },
    
    { &setup_81, &test_81_0, &test_81_1, "open", "open", &cleanup },
    
    { &setup_82, &test_82_0, &test_82_1, "open", "open", &cleanup },
    
    { &setup_83, &test_83_0, &test_83_1, "open", "open", &cleanup },
    
    { &setup_84, &test_84_0, &test_84_1, "open", "open", &cleanup },
    
    { &setup_85, &test_85_0, &test_85_1, "open", "read", &cleanup },
    
    { &setup_86, &test_86_0, &test_86_1, "open", "read", &cleanup },
    
    { &setup_87, &test_87_0, &test_87_1, "open", "read", &cleanup },
    
    { &setup_88, &test_88_0, &test_88_1, "open", "read", &cleanup },
    
    { &setup_89, &test_89_0, &test_89_1, "open", "read", &cleanup },
    
    { &setup_90, &test_90_0, &test_90_1, "open", "read", &cleanup },
    
    { &setup_91, &test_91_0, &test_91_1, "open", "read", &cleanup },
    
    { &setup_92, &test_92_0, &test_92_1, "open", "read", &cleanup },
    
    { &setup_93, &test_93_0, &test_93_1, "open", "read", &cleanup },
    
    { &setup_94, &test_94_0, &test_94_1, "open", "read", &cleanup },
    
    { &setup_95, &test_95_0, &test_95_1, "open", "read", &cleanup },
    
    { &setup_96, &test_96_0, &test_96_1, "open", "read", &cleanup },
    
    { &setup_97, &test_97_0, &test_97_1, "open", "read", &cleanup },
    
    { &setup_98, &test_98_0, &test_98_1, "open", "read", &cleanup },
    
    { &setup_99, &test_99_0, &test_99_1, "open", "read", &cleanup },
    
    { &setup_100, &test_100_0, &test_100_1, "open", "read", &cleanup },
    
    { &setup_101, &test_101_0, &test_101_1, "open", "read", &cleanup },
    
    { &setup_102, &test_102_0, &test_102_1, "open", "read", &cleanup },
    
    { &setup_103, &test_103_0, &test_103_1, "open", "write", &cleanup },
    
    { &setup_104, &test_104_0, &test_104_1, "open", "write", &cleanup },
    
    { &setup_105, &test_105_0, &test_105_1, "open", "write", &cleanup },
    
    { &setup_106, &test_106_0, &test_106_1, "open", "write", &cleanup },
    
    { &setup_107, &test_107_0, &test_107_1, "open", "write", &cleanup },
    
    { &setup_108, &test_108_0, &test_108_1, "open", "write", &cleanup },
    
    { &setup_109, &test_109_0, &test_109_1, "open", "write", &cleanup },
    
    { &setup_110, &test_110_0, &test_110_1, "open", "write", &cleanup },
    
    { &setup_111, &test_111_0, &test_111_1, "open", "write", &cleanup },
    
    { &setup_112, &test_112_0, &test_112_1, "open", "write", &cleanup },
    
    { &setup_113, &test_113_0, &test_113_1, "open", "write", &cleanup },
    
    { &setup_114, &test_114_0, &test_114_1, "open", "write", &cleanup },
    
    { &setup_115, &test_115_0, &test_115_1, "open", "write", &cleanup },
    
    { &setup_116, &test_116_0, &test_116_1, "open", "write", &cleanup },
    
    { &setup_117, &test_117_0, &test_117_1, "open", "write", &cleanup },
    
    { &setup_118, &test_118_0, &test_118_1, "open", "write", &cleanup },
    
    { &setup_119, &test_119_0, &test_119_1, "open", "write", &cleanup },
    
    { &setup_120, &test_120_0, &test_120_1, "open", "write", &cleanup },
    
    { &setup_121, &test_121_0, &test_121_1, "open", "unlink", &cleanup },
    
    { &setup_122, &test_122_0, &test_122_1, "open", "unlink", &cleanup },
    
    { &setup_123, &test_123_0, &test_123_1, "open", "unlink", &cleanup },
    
    { &setup_124, &test_124_0, &test_124_1, "open", "unlink", &cleanup },
    
    { &setup_125, &test_125_0, &test_125_1, "open", "unlink", &cleanup },
    
    { &setup_126, &test_126_0, &test_126_1, "open", "unlink", &cleanup },
    
    { &setup_127, &test_127_0, &test_127_1, "open", "unlink", &cleanup },
    
    { &setup_128, &test_128_0, &test_128_1, "open", "unlink", &cleanup },
    
    { &setup_129, &test_129_0, &test_129_1, "open", "unlink", &cleanup },
    
    { &setup_130, &test_130_0, &test_130_1, "open", "unlink", &cleanup },
    
    { &setup_131, &test_131_0, &test_131_1, "open", "unlink", &cleanup },
    
    { &setup_132, &test_132_0, &test_132_1, "open", "unlink", &cleanup },
    
    { &setup_133, &test_133_0, &test_133_1, "open", "unlink", &cleanup },
    
    { &setup_134, &test_134_0, &test_134_1, "open", "unlink", &cleanup },
    
    { &setup_135, &test_135_0, &test_135_1, "open", "unlink", &cleanup },
    
    { &setup_136, &test_136_0, &test_136_1, "open", "unlink", &cleanup },
    
    { &setup_137, &test_137_0, &test_137_1, "open", "unlink", &cleanup },
    
    { &setup_138, &test_138_0, &test_138_1, "open", "unlink", &cleanup },
    
    { &setup_139, &test_139_0, &test_139_1, "open", "unlink", &cleanup },
    
    { &setup_140, &test_140_0, &test_140_1, "open", "unlink", &cleanup },
    
    { &setup_141, &test_141_0, &test_141_1, "open", "unlink", &cleanup },
    
    { &setup_142, &test_142_0, &test_142_1, "open", "unlink", &cleanup },
    
    { &setup_143, &test_143_0, &test_143_1, "open", "unlink", &cleanup },
    
    { &setup_144, &test_144_0, &test_144_1, "open", "unlink", &cleanup },
    
    { &setup_145, &test_145_0, &test_145_1, "open", "unlink", &cleanup },
    
    { &setup_146, &test_146_0, &test_146_1, "open", "unlink", &cleanup },
    
    { &setup_147, &test_147_0, &test_147_1, "open", "link", &cleanup },
    
    { &setup_148, &test_148_0, &test_148_1, "open", "link", &cleanup },
    
    { &setup_149, &test_149_0, &test_149_1, "open", "link", &cleanup },
    
    { &setup_150, &test_150_0, &test_150_1, "open", "link", &cleanup },
    
    { &setup_151, &test_151_0, &test_151_1, "open", "link", &cleanup },
    
    { &setup_152, &test_152_0, &test_152_1, "open", "link", &cleanup },
    
    { &setup_153, &test_153_0, &test_153_1, "open", "link", &cleanup },
    
    { &setup_154, &test_154_0, &test_154_1, "open", "link", &cleanup },
    
    { &setup_155, &test_155_0, &test_155_1, "open", "link", &cleanup },
    
    { &setup_156, &test_156_0, &test_156_1, "open", "link", &cleanup },
    
    { &setup_157, &test_157_0, &test_157_1, "open", "link", &cleanup },
    
    { &setup_158, &test_158_0, &test_158_1, "open", "link", &cleanup },
    
    { &setup_159, &test_159_0, &test_159_1, "open", "link", &cleanup },
    
    { &setup_160, &test_160_0, &test_160_1, "open", "link", &cleanup },
    
    { &setup_161, &test_161_0, &test_161_1, "open", "link", &cleanup },
    
    { &setup_162, &test_162_0, &test_162_1, "open", "link", &cleanup },
    
    { &setup_163, &test_163_0, &test_163_1, "open", "link", &cleanup },
    
    { &setup_164, &test_164_0, &test_164_1, "open", "link", &cleanup },
    
    { &setup_165, &test_165_0, &test_165_1, "open", "link", &cleanup },
    
    { &setup_166, &test_166_0, &test_166_1, "open", "link", &cleanup },
    
    { &setup_167, &test_167_0, &test_167_1, "open", "link", &cleanup },
    
    { &setup_168, &test_168_0, &test_168_1, "open", "link", &cleanup },
    
    { &setup_169, &test_169_0, &test_169_1, "open", "link", &cleanup },
    
    { &setup_170, &test_170_0, &test_170_1, "open", "link", &cleanup },
    
    { &setup_171, &test_171_0, &test_171_1, "open", "link", &cleanup },
    
    { &setup_172, &test_172_0, &test_172_1, "open", "link", &cleanup },
    
    { &setup_173, &test_173_0, &test_173_1, "open", "link", &cleanup },
    
    { &setup_174, &test_174_0, &test_174_1, "open", "rename", &cleanup },
    
    { &setup_175, &test_175_0, &test_175_1, "open", "rename", &cleanup },
    
    { &setup_176, &test_176_0, &test_176_1, "open", "rename", &cleanup },
    
    { &setup_177, &test_177_0, &test_177_1, "open", "rename", &cleanup },
    
    { &setup_178, &test_178_0, &test_178_1, "open", "rename", &cleanup },
    
    { &setup_179, &test_179_0, &test_179_1, "open", "rename", &cleanup },
    
    { &setup_180, &test_180_0, &test_180_1, "open", "rename", &cleanup },
    
    { &setup_181, &test_181_0, &test_181_1, "open", "rename", &cleanup },
    
    { &setup_182, &test_182_0, &test_182_1, "open", "rename", &cleanup },
    
    { &setup_183, &test_183_0, &test_183_1, "open", "rename", &cleanup },
    
    { &setup_184, &test_184_0, &test_184_1, "open", "rename", &cleanup },
    
    { &setup_185, &test_185_0, &test_185_1, "open", "rename", &cleanup },
    
    { &setup_186, &test_186_0, &test_186_1, "open", "rename", &cleanup },
    
    { &setup_187, &test_187_0, &test_187_1, "open", "rename", &cleanup },
    
    { &setup_188, &test_188_0, &test_188_1, "open", "rename", &cleanup },
    
    { &setup_189, &test_189_0, &test_189_1, "open", "rename", &cleanup },
    
    { &setup_190, &test_190_0, &test_190_1, "open", "rename", &cleanup },
    
    { &setup_191, &test_191_0, &test_191_1, "open", "rename", &cleanup },
    
    { &setup_192, &test_192_0, &test_192_1, "open", "rename", &cleanup },
    
    { &setup_193, &test_193_0, &test_193_1, "open", "rename", &cleanup },
    
    { &setup_194, &test_194_0, &test_194_1, "open", "rename", &cleanup },
    
    { &setup_195, &test_195_0, &test_195_1, "open", "rename", &cleanup },
    
    { &setup_196, &test_196_0, &test_196_1, "open", "rename", &cleanup },
    
    { &setup_197, &test_197_0, &test_197_1, "open", "rename", &cleanup },
    
    { &setup_198, &test_198_0, &test_198_1, "open", "rename", &cleanup },
    
    { &setup_199, &test_199_0, &test_199_1, "open", "rename", &cleanup },
    
    { &setup_200, &test_200_0, &test_200_1, "open", "rename", &cleanup },
    
    { &setup_201, &test_201_0, &test_201_1, "open", "rename", &cleanup },
    
    { &setup_202, &test_202_0, &test_202_1, "open", "rename", &cleanup },
    
    { &setup_203, &test_203_0, &test_203_1, "open", "rename", &cleanup },
    
    { &setup_204, &test_204_0, &test_204_1, "open", "rename", &cleanup },
    
    { &setup_205, &test_205_0, &test_205_1, "open", "rename", &cleanup },
    
    { &setup_206, &test_206_0, &test_206_1, "open", "rename", &cleanup },
    
    { &setup_207, &test_207_0, &test_207_1, "open", "rename", &cleanup },
    
    { &setup_208, &test_208_0, &test_208_1, "open", "rename", &cleanup },
    
    { &setup_209, &test_209_0, &test_209_1, "open", "rename", &cleanup },
    
    { &setup_210, &test_210_0, &test_210_1, "open", "rename", &cleanup },
    
    { &setup_211, &test_211_0, &test_211_1, "open", "rename", &cleanup },
    
    { &setup_212, &test_212_0, &test_212_1, "open", "rename", &cleanup },
    
    { &setup_213, &test_213_0, &test_213_1, "read", "read", &cleanup },
    
    { &setup_214, &test_214_0, &test_214_1, "read", "read", &cleanup },
    
    { &setup_215, &test_215_0, &test_215_1, "read", "read", &cleanup },
    
    { &setup_216, &test_216_0, &test_216_1, "read", "read", &cleanup },
    
    { &setup_217, &test_217_0, &test_217_1, "read", "write", &cleanup },
    
    { &setup_218, &test_218_0, &test_218_1, "read", "write", &cleanup },
    
    { &setup_219, &test_219_0, &test_219_1, "read", "write", &cleanup },
    
    { &setup_220, &test_220_0, &test_220_1, "read", "write", &cleanup },
    
    { &setup_221, &test_221_0, &test_221_1, "read", "unlink", &cleanup },
    
    { &setup_222, &test_222_0, &test_222_1, "read", "unlink", &cleanup },
    
    { &setup_223, &test_223_0, &test_223_1, "read", "unlink", &cleanup },
    
    { &setup_224, &test_224_0, &test_224_1, "read", "unlink", &cleanup },
    
    { &setup_225, &test_225_0, &test_225_1, "read", "unlink", &cleanup },
    
    { &setup_226, &test_226_0, &test_226_1, "read", "unlink", &cleanup },
    
    { &setup_227, &test_227_0, &test_227_1, "read", "link", &cleanup },
    
    { &setup_228, &test_228_0, &test_228_1, "read", "link", &cleanup },
    
    { &setup_229, &test_229_0, &test_229_1, "read", "link", &cleanup },
    
    { &setup_230, &test_230_0, &test_230_1, "read", "link", &cleanup },
    
    { &setup_231, &test_231_0, &test_231_1, "read", "link", &cleanup },
    
    { &setup_232, &test_232_0, &test_232_1, "read", "link", &cleanup },
    
    { &setup_233, &test_233_0, &test_233_1, "read", "rename", &cleanup },
    
    { &setup_234, &test_234_0, &test_234_1, "read", "rename", &cleanup },
    
    { &setup_235, &test_235_0, &test_235_1, "read", "rename", &cleanup },
    
    { &setup_236, &test_236_0, &test_236_1, "read", "rename", &cleanup },
    
    { &setup_237, &test_237_0, &test_237_1, "read", "rename", &cleanup },
    
    { &setup_238, &test_238_0, &test_238_1, "read", "rename", &cleanup },
    
    { &setup_239, &test_239_0, &test_239_1, "read", "rename", &cleanup },
    
    { &setup_240, &test_240_0, &test_240_1, "read", "rename", &cleanup },
    
    { &setup_241, &test_241_0, &test_241_1, "write", "write", &cleanup },
    
    { &setup_242, &test_242_0, &test_242_1, "write", "write", &cleanup },
    
    { &setup_243, &test_243_0, &test_243_1, "write", "write", &cleanup },
    
    { &setup_244, &test_244_0, &test_244_1, "write", "write", &cleanup },
    
    { &setup_245, &test_245_0, &test_245_1, "write", "unlink", &cleanup },
    
    { &setup_246, &test_246_0, &test_246_1, "write", "unlink", &cleanup },
    
    { &setup_247, &test_247_0, &test_247_1, "write", "unlink", &cleanup },
    
    { &setup_248, &test_248_0, &test_248_1, "write", "unlink", &cleanup },
    
    { &setup_249, &test_249_0, &test_249_1, "write", "unlink", &cleanup },
    
    { &setup_250, &test_250_0, &test_250_1, "write", "unlink", &cleanup },
    
    { &setup_251, &test_251_0, &test_251_1, "write", "link", &cleanup },
    
    { &setup_252, &test_252_0, &test_252_1, "write", "link", &cleanup },
    
    { &setup_253, &test_253_0, &test_253_1, "write", "link", &cleanup },
    
    { &setup_254, &test_254_0, &test_254_1, "write", "link", &cleanup },
    
    { &setup_255, &test_255_0, &test_255_1, "write", "link", &cleanup },
    
    { &setup_256, &test_256_0, &test_256_1, "write", "link", &cleanup },
    
    { &setup_257, &test_257_0, &test_257_1, "write", "rename", &cleanup },
    
    { &setup_258, &test_258_0, &test_258_1, "write", "rename", &cleanup },
    
    { &setup_259, &test_259_0, &test_259_1, "write", "rename", &cleanup },
    
    { &setup_260, &test_260_0, &test_260_1, "write", "rename", &cleanup },
    
    { &setup_261, &test_261_0, &test_261_1, "write", "rename", &cleanup },
    
    { &setup_262, &test_262_0, &test_262_1, "write", "rename", &cleanup },
    
    { &setup_263, &test_263_0, &test_263_1, "write", "rename", &cleanup },
    
    { &setup_264, &test_264_0, &test_264_1, "write", "rename", &cleanup },
    
    { &setup_265, &test_265_0, &test_265_1, "unlink", "unlink", &cleanup },
    
    { &setup_266, &test_266_0, &test_266_1, "unlink", "unlink", &cleanup },
    
    { &setup_267, &test_267_0, &test_267_1, "unlink", "unlink", &cleanup },
    
    { &setup_268, &test_268_0, &test_268_1, "unlink", "unlink", &cleanup },
    
    { &setup_269, &test_269_0, &test_269_1, "unlink", "unlink", &cleanup },
    
    { &setup_270, &test_270_0, &test_270_1, "unlink", "unlink", &cleanup },
    
    { &setup_271, &test_271_0, &test_271_1, "unlink", "unlink", &cleanup },
    
    { &setup_272, &test_272_0, &test_272_1, "unlink", "unlink", &cleanup },
    
    { &setup_273, &test_273_0, &test_273_1, "unlink", "unlink", &cleanup },
    
    { &setup_274, &test_274_0, &test_274_1, "unlink", "unlink", &cleanup },
    
    { &setup_275, &test_275_0, &test_275_1, "unlink", "link", &cleanup },
    
    { &setup_276, &test_276_0, &test_276_1, "unlink", "link", &cleanup },
    
    { &setup_277, &test_277_0, &test_277_1, "unlink", "link", &cleanup },
    
    { &setup_278, &test_278_0, &test_278_1, "unlink", "link", &cleanup },
    
    { &setup_279, &test_279_0, &test_279_1, "unlink", "link", &cleanup },
    
    { &setup_280, &test_280_0, &test_280_1, "unlink", "link", &cleanup },
    
    { &setup_281, &test_281_0, &test_281_1, "unlink", "link", &cleanup },
    
    { &setup_282, &test_282_0, &test_282_1, "unlink", "link", &cleanup },
    
    { &setup_283, &test_283_0, &test_283_1, "unlink", "link", &cleanup },
    
    { &setup_284, &test_284_0, &test_284_1, "unlink", "rename", &cleanup },
    
    { &setup_285, &test_285_0, &test_285_1, "unlink", "rename", &cleanup },
    
    { &setup_286, &test_286_0, &test_286_1, "unlink", "rename", &cleanup },
    
    { &setup_287, &test_287_0, &test_287_1, "unlink", "rename", &cleanup },
    
    { &setup_288, &test_288_0, &test_288_1, "unlink", "rename", &cleanup },
    
    { &setup_289, &test_289_0, &test_289_1, "unlink", "rename", &cleanup },
    
    { &setup_290, &test_290_0, &test_290_1, "unlink", "rename", &cleanup },
    
    { &setup_291, &test_291_0, &test_291_1, "unlink", "rename", &cleanup },
    
    { &setup_292, &test_292_0, &test_292_1, "unlink", "rename", &cleanup },
    
    { &setup_293, &test_293_0, &test_293_1, "unlink", "rename", &cleanup },
    
    { &setup_294, &test_294_0, &test_294_1, "unlink", "rename", &cleanup },
    
    { &setup_295, &test_295_0, &test_295_1, "unlink", "rename", &cleanup },
    
    { &setup_296, &test_296_0, &test_296_1, "unlink", "rename", &cleanup },
    
    { &setup_297, &test_297_0, &test_297_1, "link", "link", &cleanup },
    
    { &setup_298, &test_298_0, &test_298_1, "link", "link", &cleanup },
    
    { &setup_299, &test_299_0, &test_299_1, "link", "link", &cleanup },
    
    { &setup_300, &test_300_0, &test_300_1, "link", "link", &cleanup },
    
    { &setup_301, &test_301_0, &test_301_1, "link", "link", &cleanup },
    
    { &setup_302, &test_302_0, &test_302_1, "link", "link", &cleanup },
    
    { &setup_303, &test_303_0, &test_303_1, "link", "link", &cleanup },
    
    { &setup_304, &test_304_0, &test_304_1, "link", "link", &cleanup },
    
    { &setup_305, &test_305_0, &test_305_1, "link", "link", &cleanup },
    
    { &setup_306, &test_306_0, &test_306_1, "link", "rename", &cleanup },
    
    { &setup_307, &test_307_0, &test_307_1, "link", "rename", &cleanup },
    
    { &setup_308, &test_308_0, &test_308_1, "link", "rename", &cleanup },
    
    { &setup_309, &test_309_0, &test_309_1, "link", "rename", &cleanup },
    
    { &setup_310, &test_310_0, &test_310_1, "link", "rename", &cleanup },
    
    { &setup_311, &test_311_0, &test_311_1, "link", "rename", &cleanup },
    
    { &setup_312, &test_312_0, &test_312_1, "link", "rename", &cleanup },
    
    { &setup_313, &test_313_0, &test_313_1, "link", "rename", &cleanup },
    
    { &setup_314, &test_314_0, &test_314_1, "link", "rename", &cleanup },
    
    { &setup_315, &test_315_0, &test_315_1, "link", "rename", &cleanup },
    
    { &setup_316, &test_316_0, &test_316_1, "link", "rename", &cleanup },
    
    { &setup_317, &test_317_0, &test_317_1, "link", "rename", &cleanup },
    
    { &setup_318, &test_318_0, &test_318_1, "rename", "rename", &cleanup },
    
    { &setup_319, &test_319_0, &test_319_1, "rename", "rename", &cleanup },
    
    { &setup_320, &test_320_0, &test_320_1, "rename", "rename", &cleanup },
    
    { &setup_321, &test_321_0, &test_321_1, "rename", "rename", &cleanup },
    
    { &setup_322, &test_322_0, &test_322_1, "rename", "rename", &cleanup },
    
    { &setup_323, &test_323_0, &test_323_1, "rename", "rename", &cleanup },
    
    { &setup_324, &test_324_0, &test_324_1, "rename", "rename", &cleanup },
    
    { &setup_325, &test_325_0, &test_325_1, "rename", "rename", &cleanup },
    
    { &setup_326, &test_326_0, &test_326_1, "rename", "rename", &cleanup },
    
    { &setup_327, &test_327_0, &test_327_1, "rename", "rename", &cleanup },
    
    { &setup_328, &test_328_0, &test_328_1, "rename", "rename", &cleanup },
    
    { &setup_329, &test_329_0, &test_329_1, "rename", "rename", &cleanup },
    
    { &setup_330, &test_330_0, &test_330_1, "rename", "rename", &cleanup },
    
    { &setup_331, &test_331_0, &test_331_1, "rename", "rename", &cleanup },
    
    { &setup_332, &test_332_0, &test_332_1, "rename", "rename", &cleanup },
    
    { &setup_333, &test_333_0, &test_333_1, "rename", "rename", &cleanup },
    
    { &setup_334, &test_334_0, &test_334_1, "rename", "rename", &cleanup },
    
    { &setup_335, &test_335_0, &test_335_1, "rename", "rename", &cleanup },
    
    { &setup_336, &test_336_0, &test_336_1, "rename", "rename", &cleanup },
    
    { 0, 0, 0 }
  };
  