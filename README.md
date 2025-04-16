This directory contains source code and other files a Multithreaded HTTP Server.

Please run this program in a **LINUX** environment. 

To run, navigate to the directory in your terminal and type 'make' to compile the program. Next type './httpserver /<port/>' where /<port/> is the desired port number in which you'd like the server to listen to incoming connections. You may also include an option '-t /<threads/>' where /<threads/> indicates the number of threads you'd like to initialize. By default, this value is 4. 

You may pipe HTTP requests however you'd like, using curl is recommended. This HTTP server has two methods, PUT and GET, which have the following format:

\<Oper\>,\<URI\>,\<Status-Code\>,\<RequestID header value\>\n

A sample GET request will look like so:

"GET /b.txt HTTP/1.1\r\nRequest-Id: 2\r\n\r\n"

A sample PUT request will look like so:

"PUT /b.txt HTTP/1.1\r\nRequest-Id: 3\r\nContent-Length: 3\r\n\r\nbye"

GET retrieves the content indicated by the /<URI/>, while PUT writes the given content to the file indicated by the /<URI/>.

Note: a PUT request will have a field "Content-Length", where the length of the content should be indicated. If none is given, the server will continue to read until there is no more content remaining. 

After an HTTP request is made, a response will appear in the audit log, located in STDERR. These responses are atomic and linearized. 
