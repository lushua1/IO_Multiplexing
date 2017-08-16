# IO_Multiplexing
linux下IO多路复用的三种机制的简单用法
 
 在使用 I/O 多路技术的时候，我们调用 select()函数和 poll()函数或epoll函数(2.6内核开始支持)，在调用它们的时候阻塞，而不是我们来调用 recvfrom（或recv）的时候阻塞。
