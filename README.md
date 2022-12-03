# MemoryManager-MultiThread_C

附加开源协议：
[![LICENSE](https://img.shields.io/badge/license-Anti%20996-blue.svg)](https://github.com/KagurazakaYukie/996-1.5/blob/master/996%E8%AE%B8%E5%8F%AF%E8%AF%81)
，商业用途可单独授权:Yukie@mroindre.com

内存管理器多线程版本

暂不开源

2022/2/24
已开源 要和这个搭配用  https://github.com/KagurazakaYukie/ThreadManager_C  ，才能做到多线程分配内存
这个是无锁设计，如果要多线程共享内存，请注意不要同时写入数据。

2022/3/20
有bug，正常用还是能用的，bug问题不大，能修，大概全都要改。修bug顺便把结构给改了，用空间换取时间。

2022/3/21
已修复,分配速度基本上，for 100次 Calloc/Free 分配和释放，无论大小，是50ns之内（intel i5 9400,ddr4 2666，Fedora35）。如果可以细分的话（也就是你知道应该怎么分配和释放更加合理），理论上还可以更快一点。

2022/12/3
释放列表存在逻辑问题，会导致违规访问，已修复
