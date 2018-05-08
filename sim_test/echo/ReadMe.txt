echo项目是windows系统下的测试程序，它实现了视频的采集、编码、传输、解码和播放的功能，它的主要工作是将视频编码后发送给relay,relay会回传给自己，这样可以在与relay之间的网络进行参数设置来
测试razor的拥塞机制是否有效。当然也可以将echo的作为relay地址，这样相当于本地socket进行通信。它包含了多个模块，详细如下：
	1.baseclasses				一个dshow依赖的库文件，由微软提供
	2.dshow						基于dshow的视频设备与采集模块
	3.h264						x.264和ffmpeg编解码器封装模块，编码器实现了根据网络状态设置码率
	4.device					视频采集与播放设备封装
	5.sim framework				sim transport传输的接口封装，实现了视频录制线程、播放线程、通信状态控制、UI消息通信机制等
	6.win_log					一个简单的windows日志接口

测试说明：
	relay运行在linux下，echo通过与relay之间的网络环路来传输视频数据。我们可以通过netem软件来控制relay的网络状态，设置延迟、丢包、抖动和乱序来调试和观察razor的运行效果，也可以
	让echo运行在弱wifi下与relay进行通信，来观察razor应对弱终端网络下的效果。
