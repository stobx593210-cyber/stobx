; ModuleID = 'StobxModule'
source_filename = "StobxModule"

@intFormat = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@stringFormat = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@trueString = private unnamed_addr constant [5 x i8] c"true\00", align 1
@falseString = private unnamed_addr constant [6 x i8] c"false\00", align 1
@0 = private unnamed_addr constant [33 x i8] c"================================\00", align 1
@1 = private unnamed_addr constant [28 x i8] c"       STOBX STUDENT SYSTEM\00", align 1
@2 = private unnamed_addr constant [33 x i8] c"================================\00", align 1
@3 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@4 = private unnamed_addr constant [21 x i8] c"TEST 1: BASIC OUTPUT\00", align 1
@5 = private unnamed_addr constant [18 x i8] c"Welcome to STOBX!\00", align 1
@6 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@7 = private unnamed_addr constant [20 x i8] c"TEST 2: MATHEMATICS\00", align 1
@8 = private unnamed_addr constant [29 x i8] c"Mathematics tests completed.\00", align 1
@9 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@10 = private unnamed_addr constant [20 x i8] c"TEST 3: COMPARISONS\00", align 1
@11 = private unnamed_addr constant [14 x i8] c"10 > 5 : PASS\00", align 1
@12 = private unnamed_addr constant [14 x i8] c"10 > 5 : FAIL\00", align 1
@13 = private unnamed_addr constant [14 x i8] c"5 < 10 : PASS\00", align 1
@14 = private unnamed_addr constant [14 x i8] c"5 < 10 : FAIL\00", align 1
@15 = private unnamed_addr constant [16 x i8] c"10 == 10 : PASS\00", align 1
@16 = private unnamed_addr constant [16 x i8] c"10 == 10 : FAIL\00", align 1
@17 = private unnamed_addr constant [14 x i8] c"10 < 5 : FAIL\00", align 1
@18 = private unnamed_addr constant [14 x i8] c"10 < 5 : PASS\00", align 1
@19 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@20 = private unnamed_addr constant [23 x i8] c"TEST 4: STUDENT RESULT\00", align 1
@21 = private unnamed_addr constant [9 x i8] c"Grade: A\00", align 1
@22 = private unnamed_addr constant [18 x i8] c"Result: Excellent\00", align 1
@23 = private unnamed_addr constant [9 x i8] c"Grade: B\00", align 1
@24 = private unnamed_addr constant [18 x i8] c"Result: Very Good\00", align 1
@25 = private unnamed_addr constant [9 x i8] c"Grade: C\00", align 1
@26 = private unnamed_addr constant [13 x i8] c"Result: Good\00", align 1
@27 = private unnamed_addr constant [9 x i8] c"Grade: D\00", align 1
@28 = private unnamed_addr constant [13 x i8] c"Result: Pass\00", align 1
@29 = private unnamed_addr constant [9 x i8] c"Grade: F\00", align 1
@30 = private unnamed_addr constant [13 x i8] c"Result: Fail\00", align 1
@31 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@32 = private unnamed_addr constant [28 x i8] c"TEST 5: MULTIPLE CONDITIONS\00", align 1
@33 = private unnamed_addr constant [13 x i8] c"Wrong branch\00", align 1
@34 = private unnamed_addr constant [24 x i8] c"Correct branch selected\00", align 1
@35 = private unnamed_addr constant [14 x i8] c"ELSE IF: PASS\00", align 1
@36 = private unnamed_addr constant [14 x i8] c"ELSE IF: FAIL\00", align 1
@37 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@38 = private unnamed_addr constant [20 x i8] c"TEST 6: ELSE BRANCH\00", align 1
@39 = private unnamed_addr constant [6 x i8] c"ERROR\00", align 1
@40 = private unnamed_addr constant [6 x i8] c"ERROR\00", align 1
@41 = private unnamed_addr constant [11 x i8] c"ELSE: PASS\00", align 1
@42 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@43 = private unnamed_addr constant [26 x i8] c"TEST 7: NESTED CONDITIONS\00", align 1
@44 = private unnamed_addr constant [22 x i8] c"Outer condition: PASS\00", align 1
@45 = private unnamed_addr constant [22 x i8] c"Inner condition: PASS\00", align 1
@46 = private unnamed_addr constant [22 x i8] c"Inner condition: FAIL\00", align 1
@47 = private unnamed_addr constant [22 x i8] c"Outer condition: FAIL\00", align 1
@48 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@49 = private unnamed_addr constant [26 x i8] c"TEST 8: MIXED EXPRESSIONS\00", align 1
@50 = private unnamed_addr constant [29 x i8] c"Mixed expressions completed.\00", align 1
@51 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@52 = private unnamed_addr constant [33 x i8] c"================================\00", align 1
@53 = private unnamed_addr constant [27 x i8] c"       STOBX TEST FINISHED\00", align 1
@54 = private unnamed_addr constant [33 x i8] c"================================\00", align 1

define i32 @main() {
entry:
  %0 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @0)
  %1 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @1)
  %2 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @2)
  %3 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @3)
  %4 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @4)
  %5 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @5)
  %6 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 2026)
  %7 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 30)
  %8 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 65)
  %9 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 42)
  %10 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 42)
  %11 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @6)
  %12 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @7)
  %13 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 70)
  %14 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 160)
  %15 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 82)
  %16 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 40)
  %17 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 25)
  %18 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @8)
  %19 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @9)
  %20 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @10)
  br i1 true, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %21 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @11)
  br label %if.end

if.else:                                          ; preds = %entry
  %22 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @12)
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  br i1 true, label %if.then1, label %if.else2

if.then1:                                         ; preds = %if.end
  %23 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @13)
  br label %if.end3

if.else2:                                         ; preds = %if.end
  %24 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @14)
  br label %if.end3

if.end3:                                          ; preds = %if.else2, %if.then1
  br i1 true, label %if.then4, label %if.else5

if.then4:                                         ; preds = %if.end3
  %25 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @15)
  br label %if.end6

if.else5:                                         ; preds = %if.end3
  %26 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @16)
  br label %if.end6

if.end6:                                          ; preds = %if.else5, %if.then4
  br i1 false, label %if.then7, label %if.else8

if.then7:                                         ; preds = %if.end6
  %27 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @17)
  br label %if.end9

if.else8:                                         ; preds = %if.end6
  %28 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @18)
  br label %if.end9

if.end9:                                          ; preds = %if.else8, %if.then7
  %29 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @19)
  %30 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @20)
  br i1 true, label %if.then10, label %if.else11

if.then10:                                        ; preds = %if.end9
  %31 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @21)
  %32 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @22)
  br label %if.end12

if.else11:                                        ; preds = %if.end9
  br i1 true, label %if.then13, label %if.else14

if.end12:                                         ; preds = %if.end15, %if.then10
  %33 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @31)
  %34 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @32)
  br i1 false, label %if.then22, label %if.else23

if.then13:                                        ; preds = %if.else11
  %35 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @23)
  %36 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @24)
  br label %if.end15

if.else14:                                        ; preds = %if.else11
  br i1 true, label %if.then16, label %if.else17

if.end15:                                         ; preds = %if.end18, %if.then13
  br label %if.end12

if.then16:                                        ; preds = %if.else14
  %37 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @25)
  %38 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @26)
  br label %if.end18

if.else17:                                        ; preds = %if.else14
  br i1 true, label %if.then19, label %if.else20

if.end18:                                         ; preds = %if.end21, %if.then16
  br label %if.end15

if.then19:                                        ; preds = %if.else17
  %39 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @27)
  %40 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @28)
  br label %if.end21

if.else20:                                        ; preds = %if.else17
  %41 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @29)
  %42 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @30)
  br label %if.end21

if.end21:                                         ; preds = %if.else20, %if.then19
  br label %if.end18

if.then22:                                        ; preds = %if.end12
  %43 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @33)
  br label %if.end24

if.else23:                                        ; preds = %if.end12
  br i1 true, label %if.then25, label %if.else26

if.end24:                                         ; preds = %if.end27, %if.then22
  %44 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @37)
  %45 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @38)
  br i1 false, label %if.then28, label %if.else29

if.then25:                                        ; preds = %if.else23
  %46 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @34)
  %47 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @35)
  br label %if.end27

if.else26:                                        ; preds = %if.else23
  %48 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @36)
  br label %if.end27

if.end27:                                         ; preds = %if.else26, %if.then25
  br label %if.end24

if.then28:                                        ; preds = %if.end24
  %49 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @39)
  br label %if.end30

if.else29:                                        ; preds = %if.end24
  br i1 false, label %if.then31, label %if.else32

if.end30:                                         ; preds = %if.end33, %if.then28
  %50 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @42)
  %51 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @43)
  br i1 true, label %if.then34, label %if.else35

if.then31:                                        ; preds = %if.else29
  %52 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @40)
  br label %if.end33

if.else32:                                        ; preds = %if.else29
  %53 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @41)
  br label %if.end33

if.end33:                                         ; preds = %if.else32, %if.then31
  br label %if.end30

if.then34:                                        ; preds = %if.end30
  %54 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @44)
  br i1 true, label %if.then37, label %if.else38

if.else35:                                        ; preds = %if.end30
  %55 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @47)
  br label %if.end36

if.end36:                                         ; preds = %if.else35, %if.end39
  %56 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @48)
  %57 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @49)
  %58 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 15)
  %59 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 20)
  %60 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 5)
  %61 = call i32 (ptr, ...) @printf(ptr @intFormat, i32 50)
  %62 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @50)
  %63 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @51)
  %64 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @52)
  %65 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @53)
  %66 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @54)
  ret i32 0

if.then37:                                        ; preds = %if.then34
  %67 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @45)
  br label %if.end39

if.else38:                                        ; preds = %if.then34
  %68 = call i32 (ptr, ...) @printf(ptr @stringFormat, ptr @46)
  br label %if.end39

if.end39:                                         ; preds = %if.else38, %if.then37
  br label %if.end36
}

declare i32 @printf(ptr, ...)
