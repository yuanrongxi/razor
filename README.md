# razor
 A google's congestion Control Algorithm
razor是一个GCC算法实现项目，主体算法来源于webRTC的CC实现，主要了解决点对点视频传输过程中网络拥塞的问题。razor是通过传输的延迟间隔来评估网络的过载与拥塞，
然后通过一个带宽调整控制器（aimd）来决策合适的码率进行视频通信。razor没有继承复杂的webRTC接口，而是将它设置在点对点视频通信的范畴。开发者可以仅仅使用razor
库和自己的传输协议来构建自己的传输体系，也可以直接使用它提供的sim transport传输系统来进行通信，这里需要说明的是sim transport还没有正式线上环境使用过，协议
的安全性没有经受过严格的考验，使用时需谨慎。

## 编译
 ### 下载
      git clone https://github.com/yuanrongxi/razor.git
 ### 编译
      用visual studio 2013打开project目录下的工程就可以直接编译，这个工程环境下有单元测试文件，可以进行代码选择修改，
      进行单元测试。
 
## 测试
 要进行razor的整体测试，需要编译sim transport模块，这里有两个工程，是sim_test下sim_sender和sim_receiver，用visual studio就可以直接进行编译运行。这
 两个工程需要配合起来使用，在sim_sender工程代码中填写sim receiver的IP地址和端口，就可以进行通信测试了。如果需要模拟网络，需要relay配合。假如运行sim sender
 和receiver机器的IP地址为192.168.1.100,relay机器的IP地址192.168.1.200:6009,那么步骤如下:
  ### 在Linux下运行relay:
      ./sim_relay 192.168.1.100:16000 192.168.1.100:16001
  ### 运行receiver
      在visual studio下运行receiver
  ### 运行sender
      在visual studio中修改sim sender中的sim_sender_test.cpp
    	if (sim_connect(1000, "192.168.1.200", 6009) != 0){
		    printf("sim connect failed!\n");
		    goto err;
	    }
      修改后直接编译运行即可.
 
 PS:也可以在linux下运行sim sender和sim receiver,直接在他们的目录下make即可编译。
 
 ## echo
   echo是一个windows下的测试程序，它据有视频传输的所有功能，需要在windows下进行编译，依赖于dshow，它同样需要relay来配合。测试步骤如下：
 ### 运行relay
    在Linux下直接运行 ./sim_relay
 ### 编译echo
    在visual studio下打开sim_test目录下的echo工程，直接编译
 ### 运行echo
    在echo UI上将receiver ip地址填写为：192.168.1.200：6009，点击start echo按钮即可
    
 # GCC与BBR的对比（来自仇波的贡献）
 ## 测试条件
  操作系统环境：VMWare虚拟机(win10)限定网速
  
  最大视频编码码率：400kBps
 ## 测试结果 
![avatar](https://github.com/yuanrongxi/razor/blob/master/doc/BBR1.jpg)

    
