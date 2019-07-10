#ifndef __COMMAND_H
#define __COMMAND_H

#define REGISTER '1'    //注册
#define LOGIN '2'       //登录
#define MESSAGE '3'     //群发消息，一般不对外公开(客户端去做)，系统管理者可以无条件，向所有在线用户发送信息
#define ADD_FRIEND '4'  //添加好友
#define ANSWER_FRIEND '5' //回应添加好友
#define GROUP_CHAT '6'   //向群组发送消息
#define SINGLE_CHAT '7' //单聊
#define SEARCH_ID '8'  //查找ID 看是否存在
#define BUILD_GROUP '9' //新建群组
#define ANSWER_BUILD_GROUP '@' //回应是否加入群组
#define ANSWER_ADD_GROUP '#'
#define SEARCH_GROUP '$'    //查询改组是否存在
#define REQUEST_ADD_GROUP '%'     //请求加组，意味着我需要一个群主
#define ANSWER_REQUEST_GROUP '&'  //回应加组请求
#define DIS_GROUP '*'  //解散群组，要求群主权限
#define ADD_USER_TO_GROUP '~'
#endif
