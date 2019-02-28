# reading-and-annotate-mongodb-3.6.1
mongodb-3.6.1源码注释分析，持续更新

QQ交流群1(针对在校生)-(群号：568892619 )
===================================    
群号：568892619  
    在校生nginx、redis、memcache、twemproxy等源码实现交流，指引在校生快速阅读这些底层开源基础中间件代码，对以后工作很有帮助  
  
QQ交流群2(针对已工作，有一些技术基础的同学)-(581089275)
===================================    
群号: 581089275  
    主要针对有一定技术基础的开发人员，提升其基础组件源码技术深度(如nginx、redis、memcache、twemproxy、mongodb、存储引擎、内核网络协议栈等)，同时可以帮助业务开发、运维开发、嵌入式等其他工作方向的同学转基础组件、中间件、分布式存储、高性能服务器等开发
  

说明:  
===================================     
MongoDB是一个基于分布式文件存储的数据库。由C++语言编写。旨在为WEB应用提供可扩展的高性能数据存储解决方案。  
是一个介于关系数据库和非关系数据库之间的产品，是非关系数据库当中功能最丰富，最像关系数据库的。他支持的数据结构非常松散，是类似json的bson格式，因此可以存储比较复杂的数据类型。Mongo最大的特点是他支持的查询语言非常强大，其语法有点类似于面向对象的查询语言，几乎可以实现类似关系数据库单表查询的绝大部分功能，而且还支持对数据建立索引。  


阅读过程  
===================================   
18.10.8  
    .提交mongo官方代码    
    .主要类结构,宏定义代码梳理注释  
    .主流程简单分析  
    .mongodb存储引擎wiredtiger代码架构梳理    
    .wiredtiger源码分析    
    .mongodb存储引擎wiredtiger源码分析:https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0   
    .添加mongodb压测工具，sysbench-mongodb    
    .网络收发处理过程完整分析  
    .网络线程模型分析  
    .集合创建过程分析  
    .insert插入过程整个过程完整分析  
    .全局锁原理分析  
    .mongodb实现wiredtier接口实现过程分析  
    .数据插入过程分析  
    .唯一索引、普通索引完整分析，分支点路径分析  

18.10.29  
    .KVCatalog分析，_mdb_catalog.wt元数据文件写入读取过程分析  
    ._mdb_catalog.wt文件作用分析  
    .FindCmd::run查找过程简要分析  
    .重新分析网络线程模型，包括adapt sync两种线程模型做详细分析  
    .网络线程模型重新分析  
    .重新梳理普通数据写入和索引写入流程，并确定其关联关系过程  
    .WiredTiger SizeStorer注释分析  
    .WiredTigerCursor注释分析  
    .wiredtiger kv engine对应的conn初始化过程分析  
    .wiredtiger_recovery_unit分析  
    .wiredtiger conn session cursor分析  
    .重新梳理网络数据接收状态转换机转换过程  
    .OperationContext重新梳理  
    .报文头部解析过程及insert操作包体解析过程分析  
    .WriteUnitOfWork事务封装过程分析  
    .document elem解析分析，_id主键添加等分析  
    .固定集合与普通集合分支走向分析，代码实现分析  
    .重新分析集合创建过程及uuid生成原理  
    .WriteUnitOfWork事务封装过程再次分析  
    .wiredtiger存储引擎接口使用分析  
    .CanonicalQuery分析  
    .QuerySolution和PlanStage生成过程分析  
    .PlanExecutor类功能分析  
    .PlanStage生成过程分析  
    .各种StageState生成过程  
    .StageState::doWork计划阶段执行过程详细分析  
    .索引选取及MultiPlanStage生成、最优索引选取  
    .按照最优的planstage执行过程分析  
    .分析wiredtiger存储引擎事务功能  
    .进一步分析wiredtiger存储引擎checkpoint实现过程  
    .结合wiredtiger存储引擎重新所里planstage中IndexScan::doWork和FetchStage::doWork实现流程，已经他们之间的关系，及索引key计数过程
    .配合wiredtiger bulk功能，分析mongo session使用该功能的代码实现  
