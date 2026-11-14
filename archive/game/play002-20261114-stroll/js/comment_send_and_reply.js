var url_param = {}
var temp = ua.split("&");
for(var i in temp){
	var t = temp[i].split("=");
	url_param[t[0]] = t[1];
}
if(navigator.userAgent.indexOf('AppleWebKit') > -1){
    //谷歌下使用外部弹出回复窗
    var reply_iframe_url = 'iframe_jf_reply.html';
/*
    if(url_param['popup']=='yes'){
        reply_iframe_url += '?popup=yes';
    }
*/
        reply_iframe_url += '?popup=yes';
    document.write('<iframe id="reply_iframe" name="reply_iframe" frameborder="0" height="0" width="100%" scrolling="no" marginheight="0" marginwidth="0" src="//'+url_param['dm']+'/httpsNot301/'+reply_iframe_url+'"></iframe>');
}else{
    if(url_param['is291']=='1')
		document.write('<iframe id="my_pl_box" name="my_pl_box" frameborder="0" height="336" width="100%" scrolling="no" marginheight="0" marginwidth="0" src="//www.4399.com/comment/www/pl3_h5ly_version.html?fid='+F_ID+'&amp;menu=1&amp;dialog=0&amp;pic=1"></iframe>');	
	else
		document.write('<iframe id="my_pl_box" name="my_pl_box" frameborder="0" height="336" width="100%" scrolling="no" marginheight="0" marginwidth="0" src="//www.4399.com/comment/www/pl3.html?fid='+F_ID+'&amp;menu=1&amp;dialog=0&amp;pic=1"></iframe>');
}