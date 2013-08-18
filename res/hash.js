b=function(b,i){
    this.s=b||0;
    this.e=i||0
}

P=function(i,a){
    var r=[];
    r[0]=i>>24&255;
    r[1]=i>>16&255;
    r[2]=i>>8&255;
    r[3]=i&255;

    for(var j=[],e=0;e<a.length;++e){
        j.push(a.charCodeAt(e));
    }

    e=[];
    e.push(new b(0,j.length-1));
    for(;e.length>0;){
        var c=e.pop();
        if(!(c.s>=c.e||c.s<0||c.e>=j.length)){
            if(c.s+1==c.e){
                if(j[c.s]>j[c.e]){
                    var l=j[c.s];
                    j[c.s]=j[c.e];
                    j[c.e]=l;
                }
            }else{
                var l=c.s;
                J=c.e;
                f=j[c.s];
                for(;c.s<c.e;){
                    for(;c.s<c.e&&j[c.e]>=f;){
                        c.e--;
                        r[0]=r[0]+3&255;
                    }
                    if(c.s<c.e){
                        j[c.s]=j[c.e];
                        c.s++;
                        r[1]=r[1]*13+43&255;
                    }
                    for(;c.s<c.e&&j[c.s]<=f;){
                        c.s++;
                        r[2]=r[2]-3&255;
                    }
                    if(c.s<c.e){
                        j[c.e]=j[c.s];
                        c.e--;
                        r[3]=(r[0]^r[1]^r[2]^r[3]+1)&255;
                    }
                }
                j[c.s]=f;
                e.push(new b(l,c.s-1));
                e.push(new b(c.s+1,J))
            }
        }
    }
    j=["0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"];
    e="";
    for(c=0;c<r.length;c++) e+=j[r[c]>>4&15],e+=j[r[c]&15];
    return e
}
