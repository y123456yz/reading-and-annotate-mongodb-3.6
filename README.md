# reading-and-annotate-mongodb-3.6.1
mongodb-3.6.1源码注释分析，持续更新

### .mongos架构性能瓶颈:  
===================================  
> * 瓶颈1、LISTEN操作由listen线程负责，只有一个线程负责listen/accept操作，在高并发场景下会成为性能瓶颈。解决办法：重复利用reuse_port功能，多个线程同时进行listen监听同一个端口    
> * 瓶颈2、net配置如果是默认配置，则每个链接会有一个单独的conn线程负责客户端请求处理，如果连接过多conn线程也相应很多，系统负载load超高，时延巨增。   
> * 拼接3、net配置如果增加了adaptive配置，mongos会根据负载情况动态调整conn线程数(实际上由worker线程而来)，在负载高的情况下同样存在大量conn线程，同时会有大量线程消耗操作，和瓶颈2一样，系统负载load超高，时延巨增。  
> * 瓶颈4、conn线程的接收到的数据会通过boost:asio异步模块处理，数据会放入一个队列，然后由NetworkInterfaceASIO线程(线程数默认为CPU数，通过连接池来解决阻塞问题)从队列取出数据转发到后端mongod复制集，队列本身就是一个瓶颈点。  
  
mongos优化方法：  
===================================  
> * 1. 利用内核reuse_port功能，多个线程监听同一个端口，实现内核连接自动分发，实现自动负载均衡。  
> * 2. 充分利用网络I/O异步多路复用功能，工作线程(进程)数只需要保持和CPU个数一致就可以了,每个线程或者进程和一个CPU绑定，线程数(多线程模式)或者进程数(多进程模式)，每个线程或进程和后端mongod复制集通过链接池来控制转发。  
> * 3. 借鉴nginx七层负载均衡机制实现跟多细节性优化，可以参考之前我的一篇文章分享:  
  
nginx多进程、高性能、低时延、高可靠机制应用于缓存中间件twemproxy，对twemproxy进行多进程优化改造，提升TPS，降低时延:  
https://my.oschina.net/u/4087916/blog/3016162  

  
mongod架构性能瓶颈:  
===================================  
> * 瓶颈1、LISTEN操作由listen线程负责，只有一个线程负责listen/accept操作，在高并发场景下会成为性能瓶颈。解决办法：重复利用reuse_port功能，多个线程同时进行listen监听同一个端口  
> * 瓶颈2、net配置如果是默认配置，则每个链接会有一个单独的conn线程负责客户端请求处理，如果连接过多conn线程也相应很多，系统负载load超高，时延巨增。  
> * 拼接3、net配置如果增加了adaptive配置，mongos会根据负载情况动态调整conn线程数(实际上由worker线程而来)，在负载高的情况下同样存在大量conn线程，同时会有大量线程消耗操作，和瓶颈2一样，系统负载load超高，时延巨增。  

  
mongod优化方法：  
===================================  
> * 1. 利用内核reuse_port功能，多个线程监听同一个端口，实现内核连接自动分发，实现自动负载均衡。  
> * 2. 由于现在的网卡很多已经是万M网卡，mongod复制集主要性能瓶颈点在磁盘I/O，万兆网卡网络I/O一般都采用异步I/O方式，所以mongod和客户端的网络处理只需要少量几个线程即可，这几个线程充分利用epoll异步事件处理、多路复用等技术即可实现数十万TPS处理(我们知道redis单线程都能实现
10万级TPS)，所以处理客户端网络数据收发的线程没必要过多，处理客户端网络数据的conn线程数(不管启用adaptive与否)，再多也没用，因为磁盘I/O才是真正的瓶颈，增加conn线程数只会进一步增加系统负载和时延。  
> * 3. 合理控制处理客户端链接的网络I/O线程数，让跟多的线程来负载处理磁盘I/O访问处理。  
> * 4. 网络I/O和磁盘I/O做分离。  
  

说明:  
===================================  
MongoDB是一个基于分布式文件存储的数据库。由C++语言编写。旨在为WEB应用提供可扩展的高性能数据存储解决方案。  
是一个介于关系数据库和非关系数据库之间的产品，是非关系数据库当中功能最丰富，最像关系数据库的。他支持的数据结构非常松散，是类似json的bson格式，因此可以存储比较复杂的数据类型。Mongo最大的特点是他支持的查询语言非常强大，其语法有点类似于面向对象的查询语言，几乎可以实现类似关系数据库单表查询的绝大部分功能，而且还支持对数据建立索引。  

mongodb存储引擎wiredtiger源码分析  
===================================  
https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0   
  
  
阅读过程  
===================================   
18.10.8  
> * .提交mongo官方代码    
> * .主要类结构,宏定义代码梳理注释  
> * .主流程简单分析  
> * .mongodb存储引擎wiredtiger代码架构梳理    
> * .wiredtiger源码分析    
> * .添加mongodb压测工具，sysbench-mongodb    
> * .网络收发处理过程完整分析  
> * .网络线程模型分析  
> * .集合创建过程分析  
> * .insert插入过程整个过程完整分析  
> * .全局锁原理分析  
> * .mongodb实现wiredtier接口实现过程分析  
> * .数据插入过程分析  
> * .唯一索引、普通索引完整分析，分支点路径分析  

18.10.29  
> * .KVCatalog分析，_mdb_catalog.wt元数据文件写入读取过程分析  
> * ._mdb_catalog.wt文件作用分析  
> * .FindCmd::run查找过程简要分析  
> * .重新分析网络线程模型，包括adapt sync两种线程模型做详细分析  
> * .网络线程模型重新分析  
> * .重新梳理普通数据写入和索引写入流程，并确定其关联关系过程  
> * .WiredTiger SizeStorer注释分析  
> * .WiredTigerCursor注释分析  
> * .wiredtiger kv engine对应的conn初始化过程分析  
> * .wiredtiger_recovery_unit分析  
> * .wiredtiger conn session cursor分析  
> * .重新梳理网络数据接收状态转换机转换过程  
> * .OperationContext重新梳理  
> * .报文头部解析过程及insert操作包体解析过程分析  
> * .WriteUnitOfWork事务封装过程分析  
> * .document elem解析分析，_id主键添加等分析  
> * .固定集合与普通集合分支走向分析，代码实现分析  
> * .重新分析集合创建过程及uuid生成原理  
> * .WriteUnitOfWork事务封装过程再次分析  
> * .wiredtiger存储引擎接口使用分析  
> * .CanonicalQuery分析  
> * .QuerySolution和PlanStage生成过程分析  
> * .PlanExecutor类功能分析  
> * .PlanStage生成过程分析  
> * .各种StageState生成过程  
> * .StageState::doWork计划阶段执行过程详细分析  
> * .索引选取及MultiPlanStage生成、最优索引选取  
> * .按照最优的planstage执行过程分析  
> * .分析wiredtiger存储引擎事务功能  
> * .进一步分析wiredtiger存储引擎checkpoint实现过程  
> * .结合wiredtiger存储引擎重新所里planstage中IndexScan::doWork和FetchStage::doWork实现流程，已经他们之间的关系，及索引key计数过程
> * .配合wiredtiger bulk功能，分析mongo session使用该功能的代码实现  
> * .mongos启动过程分析  
> * .ASIO 网络模型adaptive动态调整worker(conn-xx)线程分析  
> * .客户端键连过程、mongos与客户端异步交换过程分析  
> * .conn-xx线程模型分析  
> * .后端线程池初始化过程、conn pool链接流程分析  
> * .NetworkInterfaceASIO-TaskExecutorPool-x-x线程处理及异步网络处理过程分析  
> * .mongos和后端mongod键连过程和数据收发过程流程分界过程分析  
> * .mongos异步发送数据到后端并接收后端数据应答处理流程分析  
> * .ftdc-utils工具使用方法，用于分析mongos.diagnostic.data日志信息，这里面是二进制的，需要该工具支持  
> * .mongos-ftdc统计过程分析  
> * .新增ftdc.log文件，里面有详细的统计信息，指定方法，需要什么信息从里面看  
> * .remote Command处理过程分析  
> * .NetworkInterfaceThreadPool task生成消费过程分析  
> * .重新梳理conn线程处理client命令runCommand执行详细过程  
> * .adaptive模式下ServiceExecutorAdaptive对应的stats计算生成过程分析  
> * .Command类命令生成过程举例分析  
> * .mongos和mongod进程insert update delete命令拥有不同的类，不同的分支，分别对应ClusterWriteCmd和WriteCommand，整个流程分支分析  
> * .conn-xx线程处理客户端请求及解析数据在内部处理后，转由NetworkInterfaceASIO-TaskExecutorPool-x-0线程处理，这个分界点代码详细分析  
> * .mongos转发管理ChunkManager相关代码分析  
> * .BatchWriteOp路由过程查找详细过程  
> * .NetworkInterfaceASIO(_getDiagnosticString_inlock)统计过程分析  
> * .BatchWriteOp::_incBatchStats mongo后端应答，conn-xx线程收到应答后的统计  
> * .conn-xx线程处理应答发送给客户端过程分析  
> * .和后端开始建立链接时候的AsyncOp，以及建立连接成功后的AsyncOp赋值过程分析  
> * .OpCounter计算过程、CmdServerStatus分析  
> * .BatchedCommandRequest构造，insert update delete对应Op处理过程  
> * .客户端认证过程点分析处理  
> * .重新梳理分析mongos命令权限控制过程  
> * .ActionType操作类型  privileges权限控制过程分析  
> * .usersInfo账号权限获取过程分析  
> * .usersInfo从mongod-cfg获取用户账号、actions等信息过程分析  
> * .用户信息解析识别过程分析   
> * .管理员权限和普通用户权限privilege不同点分析  
> * .建表权限控制过程分析，分析发现只要有建表权限或者Insert权限，则默认都可以建表？insert权限为什么可以建索引？？？？？？有点好奇
> * .mongod写入过程重新分析  
> * .locker锁操作过程原理分析  
> * .locker sem信号量实现原理详细分析  
> * .locker类与Lock类区别、及其他们的关联性分析  
> * .客户端获取锁过程及锁状态变换过程分析，globalStats全局lock统计过程分析  
> * .重新梳理用户权限创建过程  
> * .getmore querey查找命令执行过程重新分析  
> * .符合索引范围查询过程的解析增加文档记录  
> * .lockManager详细注释分析  
> * .AutoGetCollection初始化构造过程及其与锁的关系分析  
> * .命令汇总
> * .
> * .
> * .
> * .
