//  http://localhost/center/server/add?server=iot/mqtt&host=127.0.0.1:8080&version=0.0.1&type=proxypass
const {promises:fs}=require('fs');
const {join}=require('path');
require('http').createServer((req, res) => {
    console.log(req.url);
    const url=new URL(req.url,"http://localhost");
    const name=url.searchParams.get('name');

    if(name){
        const file=join('file',name);
        req.pipe(require("fs").createWriteStream(file)).once('close',async()=>{
            await new Promise(rel=>setTimeout(rel,200));
            const sta=await fs.stat(file);
            console.log(sta.size);
            res.end(sta.size.toString());
            // res.end(req.url);
        });
    }else{
        res.end(req.url);
    }
}).listen(7788);