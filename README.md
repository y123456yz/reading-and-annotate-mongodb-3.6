#reading-and-annotate-mongodb-3.6


mongodb-3.6源码注释分析，持续更新

===================================     
### 对外演讲   
|#|对外演讲|演讲内容|
|:-|:-|:-|
|1|Qcon全球软件开发大会分享|[OPPO万亿级文档数据库MongoDB集群性能优化实践](https://qcon.infoq.cn/2020/shenzhen/track/916)|
|2|2021年度Gdevops全球敏捷运维峰会|[PB级万亿数据库性能优化及最佳实践](https://gdevops.com/index.php?m=content&c=index&a=lists&catid=87)|
|3|2019年mongodb年终盛会|[OPPO百万级高并发MongoDB集群性能数十倍提升优化实践](https://www.shangyexinzhi.com/article/428874.html)|
|4|2020年mongodb年终盛会|[万亿级文档数据库集群性能优化实践](https://mongoing.com/archives/76151)|
|5|2021年dbaplus分享|[万亿级文档数据库集群性能优化实践](http://dbaplus.cn/news-162-3666-1.html)|

  
### 专栏  
|#|专栏名|专栏内容|
|:-|:-|:-|
|1|infoq专栏|[《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|2|oschina专栏|[《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://my.oschina.net/u/4087916)|
|3|知乎专栏|[《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|4|itpub专栏|[《mongodb内核源码设计实现、性能优化、最佳运维实践》](http://blog.itpub.net/column/150)|

### 《mongodb内核源码设计与实现》源码模块化分析  
#### 第一阶段：单机内核源码分析  
![单机模块化架构图](/image/单机模块化架构图.png)  
|#|单机模块名|核心代码中文注释|说明|模块文档输出|
|:-|:-|:-|:-|:-|
|1|[网络收发处理(含工作线程模型)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L8)|网络处理模块核心代码实现(100%注释分析)|完成ASIO库、网络数据收发、同步线程模型、动态线程池模型等功能|[详见infoq专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|2|[command命令处理模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L85)|命令处理相关模块源码分析(100%注释分析)|完成命令注册、命令执行、命令分析、命令统计等功能|[详见oschina专栏:《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|3|[write写(增删改操作)模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L115))|增删改写模块(100%注释分析)|完成增删改对应命令解析回调处理、事务封装、storage存储模块对接等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|4|[query查询引擎模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L131))|query查询引擎模块(核心代码注释)|完成expression tree解析优化处理、querySolution生成、最优索引选择等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|5|[concurrency并发控制模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/tree/master/mongo/src/mongo/db/concurrency)|并发控制模块(核心代码注释)|完成信号量、读写锁、读写意向锁相关实现及封装|[详见infoq专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|6|[index索引模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L240)|index索引模块(100%注释分析)|完成索引解析、索引管理、索引创建、文件排序等功能|[详见oschina专栏:《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|7|[storage存储模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L115))|storage存储模块(100%注释分析)|完成存储引擎注册、引擎选择、中间层实现、KV实现、wiredtiger接口实现等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|8|[wiredtiger存储引擎](https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0)) |wiredtiger存储引擎设计与实现专栏分析(已分析部分)|完成KV读写、存储结构、checkpoint择等主功能，待完善|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0)|
  
  
#### 第二阶段：复制集内核源码分析(已分析部分源码，待整理,持续分析)  
    
    
#### 第三阶段：sharding分片内核源码分析(已分析部分源码，待整理，持续分析)   
      
#### 第四阶段：wiredtiger存储引擎源码分析(已分析部分源码，待整理，持续分析)  
  
#### 第五阶段：重新回顾分析mongodb内核主模块以外细节(已分析部分源码，待整理，持续分析) 
   
### <<千万级峰值tps/十万亿级数据量文档数据库内核研发及运维之路>>   
|#|文章内容|
|:-|:-|
|1|[盘点 2020 - 我要为分布式数据库 mongodb 在国内影响力提升及推广做点事](https://xie.infoq.cn/article/372320c6bb93ddc5b7ecd0b6b)|
|2|[万亿级数据库 MongoDB 集群性能数十倍提升及机房多活容灾实践](https://xie.infoq.cn/article/304a748ad3dead035a449bd51)|
|3|[Qcon现代数据架构 -《万亿级数据库 MongoDB 集群性能数十倍提升优化实践》核心 17 问详细解答](https://xie.infoq.cn/article/0c51f3951f3f10671d7d7123e)|
|4|[数百万级代码量mongodb内核源码阅读经验分享](https://xie.infoq.cn/article/7b2c1dc67de82972faac2812c)|
|5|[话题讨论 - mongodb 相比 mysql 拥有十大核心优势，为何国内知名度不高？](https://xie.infoq.cn/article/180d98535bfa0c3e71aff1662)|
|6|[万亿级数据库 MongoDB 集群性能数十倍提升及机房多活容灾实践](https://xie.infoq.cn/article/304a748ad3dead035a449bd51)|
|7|[百万级高并发mongodb集群性能数十倍提升优化实践(上篇)](https://my.oschina.net/u/4087916/blog/3141909)|
|8|[百万级高并发mongodb集群性能数十倍提升优化实践(下篇)](https://my.oschina.net/u/4087916/blog/3155205)|
|9|[Mongodb网络传输处理源码实现及性能调优-体验内核性能极致设计](https://my.oschina.net/u/4087916/blog/4295038)|
|10|[常用高并发网络线程模型设计及mongodb线程模型优化实践(最全高并发网络IO线程模型设计及优化)](https://my.oschina.net/u/4087916/blog/4431422) |
|11|[Mongodb集群搭建一篇就够了-复制集模式、分片模式、带认证、不带认证等(带详细步骤说明)](https://my.oschina.net/u/4087916/blog/4661542)|
|12|[Mongodb特定场景性能数十倍提升优化实践(记一次mongodb核心集群雪崩故障)](https://blog.51cto.com/14951246)|
|13|[mongodb内核源码设计实现、性能优化、最佳运维系列-mongodb网络传输层模块源码实现二](https://zhuanlan.zhihu.com/p/265701877)|
|14|[为何需要对开源mongodb社区版本做二次开发，需要做哪些必备二次开发](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/development_mongodb.md)|
|15|[对开源mongodb社区版本做二次开发收益列表](https://my.oschina.net/u/4087916/blog/3063529)|
|16|[盘点 2020 - 我要为分布式数据库 mongodb 在国内影响力提升及推广做点事](https://xie.infoq.cn/article/372320c6bb93ddc5b7ecd0b6b)|
|17|[300 条数据变更引发的血案 - 记某十亿级核心 mongodb 集群部分请求不可用故障踩坑记](https://xie.infoq.cn/article/5932858d57db13d43a8b8d62a)|  
|18|[记十亿级Es数据迁移mongodb成本节省及性能优化实践](https://zhuanlan.zhihu.com/p/373351625)|  
|19|[千亿级数据迁移mongodb成本节省及性能优化实践](https://zhuanlan.zhihu.com/p/376679225)|  
|20|[千亿级数据迁移 mongodb 成本节省及性能优化实践 (附性能对比质疑解答)](https://xie.infoq.cn/article/2bc78d36adef6832ada8ea7c5)|  
|21|[记某百亿级 mongodb 集群数据过期性能优化实践](https://xie.infoq.cn/article/98daf7330a3107fa0bf1edc9c)|  
|27|[mongodb内核源码实现、性能调优、最佳运维实践系列-数百万行mongodb内核源码阅读经验分享](https://my.oschina.net/u/4087916/blog/4696104)|  
|28|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现一](https://my.oschina.net/u/4087916/blog/4295038)|
|29|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现二](https://my.oschina.net/u/4087916/blog/4674521)|
|30|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现三](https://my.oschina.net/u/4087916/blog/4678616)|
|31|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现四](https://my.oschina.net/u/4087916/blog/4685419)|
|32|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现一](https://my.oschina.net/u/4087916/blog/4709503)|
|33|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现二](https://my.oschina.net/u/4087916/blog/4748286)|
|34|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现三](https://my.oschina.net/u/4087916/blog/4782741)|
|35|[mongodb内核源码实现、性能调优、最佳运维实践系列-记mongodb详细表级操作及详细时延统计实现原理(教你如何快速进行表级时延问题分析)](https://xie.infoq.cn/article/3184cdc42c26c86e2749c3e5c)|
|36|[mongodb内核源码实现、性能调优、最佳运维实践系列-Mongodb write写(增、删、改)模块设计与实现](https://my.oschina.net/u/4087916/blog/4974132)|
  
     
### 其他  
#### nginx高并发设计优秀思想应用于其他高并发代理中间件:   
  * [高性能 -Nginx 多进程高并发、低时延、高可靠机制在百万级缓存 (redis、memcache) 代理中间件中的应用](https://xie.infoq.cn/article/2ee961483c66a146709e7e861)  

#### redis、nginx、memcache、twemproxy、mongodb等更多中间件，分布式系统，高性能服务端核心思想实现博客:    
  * [中间件、高性能服务器、分布式存储等(redis、memcache、pika、rocksdb、mongodb、wiredtiger、高性能代理中间件)二次开发、性能优化，逐步整理文档说明并配合demo指导](https://github.com/y123456yz/middleware_development_learning)    
      

说明:  
===================================  
MongoDB是一个基于分布式文件存储的数据库。由C++语言编写。旨在为WEB应用提供可扩展的高性能数据存储解决方案。  
是一个介于关系数据库和非关系数据库之间的产品，是非关系数据库当中功能最丰富，最像关系数据库的。他支持的数据结构非常松散，是类似json的bson格式，因此可以存储比较复杂的数据类型。Mongo最大的特点是他支持的查询语言非常强大，其语法有点类似于面向对象的查询语言，几乎可以实现类似关系数据库单表查询的绝大部分功能，而且还支持对数据建立索引。  
  
源码中文已注释代码列表如下：
===================================   
#### boost-asio网络库/定时器源码实现注释(只注释mongodb相关实现的asio库代码)(100%注释):   
 *   [asio/include/asio/detail/impl/scheduler.ipp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/impl/scheduler.ipp) 
 *   [asio/include/asio/detail/impl/epoll_reactor.ipp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/impl/epoll_reactor.ipp) 
 *   [asio/include/asio/detail/scheduler.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/scheduler.hpp) 
 *   [asio/include/asio/detail/impl/scheduler.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/impl/scheduler.hpp) 
 *   [asio/include/asio/detail/timer_queue.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/timer_queue.hpp) 
 *   [asio/include/asio/detail/timer_queue_base.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/timer_queue_base.hpp) 
 *   [asio/include/asio/detail/timer_queue_set.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/timer_queue_set.hpp) 
 *   [asio/include/asio/detail/impl/timer_queue_set.ipp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/impl/timer_queue_set.ipp) 
 *   [asio/include/asio/detail/epoll_reactor.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/epoll_reactor.hpp) 
 *   [asio/include/asio/detail/impl/epoll_reactor.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/impl/epoll_reactor.hpp) 
 *   [asio/include/asio/impl/read.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/impl/read.hpp) 
 *   [asio/include/asio/impl/write.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/impl/write.hpp) 
 *   [asio/include/asio/basic_socket_acceptor.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation//asio/include/asio/basic_socket_acceptor.hpp) 
 *   [asio/include/asio/detail/reactive_socket_service.hpp)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/reactive_socket_service.hpp) 
 *   [asio/include/asio/basic_socket_acceptor.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/basic_socket_acceptor.hpp) 
 *   [asio/include/asio/basic_stream_socket.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/basic_stream_socket.hpp) 
 *   [asio/include/asio/detail/reactive_socket_service_base.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/reactive_socket_service_base.hpp) 
 *   [asio/include/asio/detail/reactive_socket_recv_op.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/reactive_socket_recv_op.hpp) 
 *   [asio/include/asio/detail/reactor_op.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/reactor_op.hpp) 
 *   [asio/include/asio/detail/scheduler_operation.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/scheduler_operation.hpp) 
 *   [asio/include/asio/detail/deadline_timer_service.hpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/third_party/asio-chinese-annotation/asio/include/asio/detail/deadline_timer_service.hpp) 

#### mongodb网络传输模块(transport)处理实现(100%注释):     
###### transport_layer传输层子模块: 
 *   [transport_layer_asio.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/transport_layer_asio.h) 
 *   [transport_layer_asio.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/transport_layer_asio.cpp) 
 *   [transport_layer_manager.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/transport_layer_manager.h) 
 *   [transport_layer_manager.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/transport_layer_manager.cpp) 
 *   [transport_layer.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/transport_layer.h) 
###### Ticket数据收发回调处理子模块(100%注释): 
 *   [ticket_asio.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/ticket_asio.h) 
 *   [ticket_asio.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/ticket_asio.cpp) 
 *   [ticket_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/ticket_impl.h) 
###### Session会话子模块(100%注释): 
 *   [session_asio.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/session_asio.h) 
 *   [session_asio.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/session_asio.cpp) 
 *   [session.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/session.h) 
###### service_state_machine状态机子模块(100%注释): 
 *   [service_state_machine.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_state_machine.h) 
 *   [service_state_machine.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_state_machine.cpp) 
###### service_executor服务运行(工作线程模型)子模块(100%注释): 
 *   [service_executor.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_executor.h) 
 *   [service_executor_adaptive.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_executor_adaptive.cpp) 
 *   [service_executor_adaptive.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_executor_adaptive.h) 
 *   [service_executor_synchronous.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_executor_synchronous.cpp) 
 *   [service_executor_synchronous.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_executor_synchronous.h) 
###### service_entry_point_impl服务入口子模块(100%注释): 
 *   [service_entry_point.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_entry_point.h) 
 *   [service_entry_point_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_entry_point_impl.cpp) 
 *   [service_entry_point_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_entry_point_impl.h) 
 *   [service_entry_point_utils.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_entry_point_utils.cpp) 
 *   [service_entry_point_utils.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/transport/service_entry_point_utils.h) 

#### message/DbMessage/OpMsgRequest协议处理(100%注释): 
 *   [message.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/net/message.h) 
 *   [message.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/net/message.cpp) 
 *   [dbmessage.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/dbmessage.h) 
 *   [dbmessage.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/dbmessage.cpp) 
 *   [factory.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/rpc/factory.cpp) 
 *   [factory.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/rpc/factory.h) 
 *   [op_msg.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/net/op_msg.cpp) 
 *   [op_msg.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/net/op_msg.h) 
 
#### 时间嘀嗒及系统级定时器实现(100%注释): 
 *   [tick_source.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/tick_source.h) 
 *   [system_tick_source.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/system_tick_source.cpp) 
 *   [system_tick_source.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/system_tick_source.h) 
 *   [timer.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/timer.cpp) 
 *   [timer.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/util/timer.h) 

#### mongod/mongos服务入口处理(100%注释): 
 *   [service_entry_point_mongod.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/service_entry_point_mongod.h) 
 *   [service_entry_point_mongod.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/service_entry_point_mongod.cpp) 
 *   [service_entry_point_mongos.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/service_entry_point_mongos.h) 
 *   [service_entry_point_mongos.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/service_entry_point_mongos.cpp)

#### command命令处理模块(注释完毕,命令太多，请参考前面的《command命令处理模块源码实现》): 
 *   [commands.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands.h) 
 *   [commands.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands.cpp) 
 *   [write_commands.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/write_commands/write_commands.cpp) 
 *   [cluster_write_cmd.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/commands/cluster_write_cmd.cpp)
 *   [strategy.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/commands/strategy.cpp)

#### db.serverStatus()相关统计(注释完毕):
 *   [server_status.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status.cpp) 
 *   [server_status.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status.h) 
 *   [server_status_internal.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status_internal.cpp) 
 *   [server_status_internal.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status_internal.h) 
 *   [server_status_metric.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status_metric.cpp) 
 *   [server_status_metric.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/server_status_metric.h) 

#### stats相关统计(注释完毕):
 *   [counters.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/counters.cpp) 
 *   [counters.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/counters.h) 
 *   [operation_latency_histogram.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/operation_latency_histogram.cpp) 
 *   [operation_latency_histogram.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/operation_latency_histogram.h) 
 *   [top.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/top.cpp) 
 *   [top.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/top.h) 
 *   [latency_server_status_section.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/latency_server_status_section.cpp) 
 *   [toplatency_server_status_sectionh](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/stats/latency_server_status_section.h) 


#### OpMsgRequest和写write_ops:(insert、update、delete)转换操作(100%注释): :
 *   [write_ops_gen.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/ops/write_ops_gen.cpp) 
 *   [write_ops_gen.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/ops/write_ops_gen.h) 

#### write写模块:  
###### write处理入口(100%注释): 
 *   [write_commands_common.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/write_commands/write_commands_common.cpp) 
 *   [write_commands_common.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/write_commands/write_commands_common.h) 
 *   [write_commands.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/commands/write_commands/write_commands.cpp) 
  
###### OpMsgRequest和写write_ops:(insert、update、delete)转换操作(100%注释): :
 *   [write_ops_parsers.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/ops/write_ops_parsers.cpp) 
 *   [write_ops_parsers.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/ops/write_ops_parsers.h) 
 *   [write_ops_gen.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/ops/write_ops_gen.cpp) 
 *   [write_ops_gen.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/ops/write_ops_gen.h) 
   
###### 增删改处理(100%注释): :
 *   [write_ops_exec.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/ops/write_ops_exec.cpp) 
 *   [write_ops_exec.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/ops/write_ops_exec.h) 
  
#### query请求处理模块:  
###### query_request请求解析和canonical_query规范化转换操作(核心流程注释完毕): 
 *   [query_request.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/query_request.cpp) 
 *   [query_request.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/query_request.h) 
 *   [canonical_query.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/canonical_query.cpp) 
 *   [canonical_query.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/canonical_query.h) 
 *   [parsed_projection.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/parsed_projection.cpp) 
 *   [parsed_projection.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/parsed_projection.h) 
 
###### MatchExpression tree生成及优化过程(核心流程注释完毕): 
 *   [expression_parser.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression_parser.cpp) 
 *   [expression_parser.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression_parser.h) 
 *   [expression.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression.cpp) 
 *   [expression.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression.h) 
 *   [expression_tree.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression_tree.cpp) 
 *   [expression_tree.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/matcher/expression_tree.h) 

###### get_executor获取PlanExecutor(核心流程注释完毕): 
 *   [get_executor.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.cpp) 
 *   [get_executor.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.h) 

###### QueryPlannerIXSelect实现MatchExpression tree相关node关联对应RelevantTag(核心流程注释完毕): 
 *   [planner_ixselect.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_ixselect.cpp) 
 *   [planner_ixselect.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_ixselect.h) 

###### plan_enumerator轮询枚举每个查询所有的候选索引信息(核心流程注释完毕): 
 *   [plan_enumerator.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_enumerator.cpp) 
 *   [plan_enumerator.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_enumerator.h) 

###### index_tag相关(核心流程注释完毕): 
 *   [index_tag.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/index_tag.cpp) 
 *   [index_tag.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/index_tag.h) 

###### 生成QuerySolutionNode tree及querysolution(核心流程注释完毕): 
 *   [planner_access.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_access.cpp) 
 *   [planner_access.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_access.h) 
 *   [planner_analysis.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_analysis.cpp) 
 *   [planner_analysis.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/planner_analysis.h) 
 *   [query_planner.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/query_planner.cpp) 
 *   [query_planner.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/query_planner.h) 

###### get_executor根据querysolurion生成PlanStage(核心流程注释完毕): 
 *   [get_executor.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.cpp) 
 *   [get_executor.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.h) 
 
###### PlanStage(核心流程注释完毕): 
 *   [plan_stage.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/plan_stage.cpp) 
 *   [plan_stage.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/plan_stage.h) 
 *   [plan_stats.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/plan_stats.cpp) 
 *   [plan_stats.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/plan_stats.h) 
 *   [stage_builder.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/stage_builder.cpp) 
 *   [stage_builder.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/stage_builder.h) 
 *   [multi_plan.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/multi_plan.cpp)   
 *   [multi_plan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/multi_plan.h)  
 *   [subplan.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/subplan.cpp)   
 *   [subplan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/subplan.h)  
 *   [stage_types.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/stage_types.cpp) 
 *   [collection_scan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/collection_scan.h) 
 *   [collection_scan.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/collection_scan.cpp) 
 *   [collection_scan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/collection_scan.h) 
 *   [index_scan.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/index_scan.cpp) 
 *   [index_scan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/index_scan.h) 
 *   [fetch.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/fetch.cpp) 
 *   [fetch.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/fetch.h) 
 *   [sort.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/sort.cpp) 
 *   [sort.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/sort.h)
 *   [sort_key_generator.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/sort_key_generator.cpp) 
 *   [sort_key_generator.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/sort_key_generator.h) 
 *   [projection.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/projection.cpp) 
 *   [projection.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/projection.h)   
 *   [limit.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/limit.cpp)   
 *   [limit.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/limit.h)   
 *   [skip.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/skip.cpp)   
 *   [skip.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/skip.h)  
 *   [cached_plan.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/cached_plan.cpp)   
 *   [cached_plan.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/cached_plan.h) 
 *   [update.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/update.cpp)   
 *   [update.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/update.h)  
 *   [delete.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/delete.cpp)   
 *   [delete.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/delete.h) 
 *   [......](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/exec/)  
  
  
#### storage存储模块:  
##### catalog中间层相关实现: 
###### database中间层接口相关(重要接口注释): 
 *   [database.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database.cpp) 
 *   [database.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database.h) 
 *   [database_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_impl.h) 
 *   [database_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_impl.cpp) 
 *   [database_holder.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_holder.h) 
 *   [database_holder.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_holder.cpp) 
 *   [database_holder_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_holder_impl.cpp) 
 *   [database_holder_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_holder_impl.h) 
 *   [database_catalog_entry.h (通过这里和KVStorageEngine关联)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/database_catalog_entry.h) 
  
###### collection中间层接口相关(重要接口注释): 
 *   [collection.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection.cpp) 
 *   [collection.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection.h) 
 *   [collection_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_impl.h) 
 *   [collection_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_impl.cpp) 
 *   [collection_info_cache.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_info_cache.h) 
 *   [collection_info_cache.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_info_cache.cpp) 
 *   [collection_info_cache_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_info_cache_impl.cpp) 
 *   [collection_info_cache_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_info_cache_impl.h) 
 *   [collection_options.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_options.cpp) 
 *   [collection_options.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_options.h) 
 *   [collection_catalog_entry.h (通过这里和KVStorageEngine关联)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/collection_catalog_entry.h) 
  
###### index中间层接口相关(重要接口注释): 
 *   [index_catalog.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog.cpp) 
 *   [index_catalog.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog.h) 
 *   [index_catalog_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_impl.h) 
 *   [index_catalog_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_impl.cpp) 
 *   [index_catalog_entry.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_entry.h) 
 *   [index_catalog_entry.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_entry.cpp) 
 *   [index_catalog_entry_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_entry_impl.cpp) 
 *   [index_catalog_entry_impl.h (通过这里和CollectionInfoCache、CollectionCatalogEntry、IndexAccessMethod等关联)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_catalog_entry_impl.h) 
 *   [index_create.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_create.cpp) 
 *   [index_create.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_create.h) 
 *   [index_create_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_create_impl.cpp) 
 *   [index_create_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/index_create_impl.h) 
  
##### storage层相关实现(重要接口注释): 
 *   [record_data.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/record_data.cpp) 
 *   [record_data.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/record_data.h) 
 *   [record_store.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/record_store.h) 
 *   [sorted_data_interface.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/sorted_data_interface.h) 
 *   [storage_engine.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_engine.h) 
 *   [storage_engine_lock_file.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_engine_lock_file.cpp) 
 *   [storage_engine_lock_file.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_engine_lock_file.h) 
 *   [storage_engine_metadata.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_engine_metadata.cpp) 
 *   [storage_engine_metadata.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_engine_metadata.h) 
 *   [storage_init.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_init.cpp) 
 *   [storage_options.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_options.h) 
 *   [storage_options.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/storage_options.cpp) 
  
###### kv引擎管理注册(重要接口注释): 
 *   [kv_storage_engine.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_storage_engine.cpp) 
 *   [kv_storage_engine.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_storage_engine.h) 
 *   [kv_catalog.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_catalog.h) 
 *   [kv_catalog.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_catalog.cpp) 
 *   [kv_collection_catalog_entry.h(通过这里和catalog中间层collection衔接)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_collection_catalog_entry.h) 
 *   [kv_collection_catalog_entry.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_collection_catalog_entry.cpp) 
 *   [kv_database_catalog_entry.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_database_catalog_entry.cpp) 
 *   [kv_database_catalog_entry.h(通过这里和catalog中间层database衔接)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_database_catalog_entry.h) 
 *   [kv_database_catalog_entry_base.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_database_catalog_entry_base.cpp) 
 *   [kv_database_catalog_entry_base.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/kv_database_catalog_entry_base.h) 
   
###### wiredtiger存储引擎接口相关(重要接口注释): 
 *   [wiredtiger_global_options.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_global_options.cpp) 
 *   [wiredtiger_global_options.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_global_options.h) 
 *   [wiredtiger_init.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_init.h) 
 *   [wiredtiger_init.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_init.cpp) 
 *   [wiredtiger_kv_engine.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_kv_engine.h) 
 *   [wiredtiger_kv_engine.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_kv_engine.cpp) 
 *   [wiredtiger_parameters.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_parameters.cpp) 
 *   [wiredtiger_parameters.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_parameters.h) 
 *   [wiredtiger_record_store.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_record_store.cpp) 
 *   [wiredtiger_record_store.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_record_store.h) 
 *   [wiredtiger_recovery_unit.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_recovery_unit.h) 
 *   [wiredtiger_recovery_unit.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_recovery_unit.cpp) 
 *   [wiredtiger_server_status.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_server_status.cpp) 
 *   [wiredtiger_server_status.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_server_status.h) 
 *   [wiredtiger_session_cache.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_session_cache.cpp) 
 *   [wiredtiger_session_cache.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_session_cache.h) 
 *   [wiredtiger_size_storer.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_size_storer.h) 
 *   [wiredtiger_size_storer.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_size_storer.cpp) 
 *   [wiredtiger_util.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_util.cpp) 
 *   [wiredtiger_util.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/catalog/wiredtiger_util.h) 

   
###### plan_ranker对每个候选solution打分，选出最优索引(核心流程注释完毕): 
 *   [plan_ranker.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_ranker.cpp) 
 *   [plan_ranker.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_ranker.h) 
 
###### get_executor根据querysolurion生成PlanStage(核心流程注释完毕): 
 *   [get_executor.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.cpp) 
 *   [get_executor.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/get_executor.h) 
 
###### plan_executor执行器(核心流程注释完毕): 
 *   [plan_executor.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_executor.cpp) 
 *   [plan_executor.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_executor.h) 
 
###### plan_cache plan缓存(核心流程注释完毕): 
 *   [plan_cache.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_cache.cpp) 
 *   [plan_cache.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/query/plan_cache.h) 
 *   [plan_cache_commands.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/commands/plan_cache_commands.cpp) 
 *   [plan_cache_commands.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/build/opt/mongo/db/commands/plan_cache_commands.h) 
 
#### shard分片源码实现(注释进行中):   
###### 分布式锁实现源码注释分析(100%注释): 
 *   [dist_lock_catalog_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog_impl.cpp) 
 *   [dist_lock_catalog_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog_impl.h) 
 *   [dist_lock_manager.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_manager.cpp) 
 *   [dist_lock_catalog.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog.h) 
 *   [dist_lock_catalog_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog_impl.cpp) 
 *   [dist_lock_catalog_impl.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog_impl.h) 
 *   [dist_lock_catalog_impl.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_catalog_impl.cpp) 
 *   [dist_lock_manager.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/dist_lock_manager.cpp) 
 *   [type_lockpings.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_lockpings.cpp) 
 *   [type_lockpings.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_lockpings.h) 
 *   [type_locks.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_locks.cpp) 
 *   [type_locks.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_locks.h) 
 *   [configsvr_enable_sharding_command.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/s/config/configsvr_enable_sharding_command.cpp) 

###### 代理定期更新config.mongos实现源码注释分析(100%注释): 
 *   [sharding_uptime_reporter.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/sharding_uptime_reporter.cpp)
 *   [sharding_uptime_reporter.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/sharding_uptime_reporter.h)
 *   [type_mongos.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_mongos.cpp)
 *   [type_mongos.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_mongos.h)

###### cfg复制集库表结构管理(config.databases、config.collections)(100%注释): 
 *   [type_collection.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_collection.cpp)
 *   [type_collection.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_collection.h)
 *   [type_database.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_database.cpp)
 *   [type_database.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/catalog/type_database.h)

###### 分片片建shard key(100%注释): 
 *   [shard_key_pattern.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/shard_key_pattern.cpp)
 *   [shard_key_pattern.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/s/shard_key_pattern.h)
 *   [keypattern.cpp](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/keypattern.cpp)
 *   [keypattern.h](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/mongo/src/mongo/db/keypattern.h)
 
mongodb存储引擎wiredtiger源码分析  
===================================  
https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0   
  

  



