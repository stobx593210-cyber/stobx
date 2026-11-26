; ModuleID = 'StobxModule'
source_filename = "StobxModule"

@intFormat = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@uintFormat = private unnamed_addr constant [4 x i8] c"%u\0A\00", align 1
@i64Format = private unnamed_addr constant [6 x i8] c"%lld\0A\00", align 1
@u64Format = private unnamed_addr constant [6 x i8] c"%llu\0A\00", align 1
@floatFormat = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1
@ptrFormat = private unnamed_addr constant [4 x i8] c"%p\0A\00", align 1
@stringFormat = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@nullString = private unnamed_addr constant [7 x i8] c"(null)\00", align 1
@trueString = private unnamed_addr constant [5 x i8] c"true\00", align 1
@falseString = private unnamed_addr constant [6 x i8] c"false\00", align 1

define i32 @main() {
entry:
  %ptr = alloca ptr, align 8
  ret i32 0
}

declare i32 @printf(ptr, ...)
