### 注意：
- 编译平台为：Linux ubuntu 4.15.0-29-generic #31~16.04.1-Ubuntu SMP Wed Jul 18 08:54:04 UTC 2018 x86_64 x86_64 x86_64 GNU/Linux
- 重新编译可以
   - make clean
   - make
- 可以这样运行
   - ./serve 10086 &     #监听10086端口
   - ./client <ip> 10086 #连接服务器
   - ls                  #查看服务器当前目录下的文件
   - pwd                 #查看当前目录
   - cd dir              #进入到别的目录
   - get filename        #获取服务器文件
   - put filename        #发送文件到服务器