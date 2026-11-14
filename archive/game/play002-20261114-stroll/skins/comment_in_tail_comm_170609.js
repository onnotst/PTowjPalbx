var _GO_=function(n){
	return document.getElementById(n);
}
var _GA_=function(n){
	var ua = "&"+window.location.search.substring(1) + "&";
	var un = "&"+n+"=";
	var ul = un.length;
	var ui = ua.indexOf(un);
	if(ui == -1)return 'www.4399.com';
    if ((ua.indexOf('.4399.com') <0) && (ua.indexOf('.5054399.com') <0) && (ua.indexOf('.4399pk.com') <0))
        return 'www.4399.com';
	var ue = ua.indexOf("&", ui+ul);
	return ua.substring(ui+ul, ue);
}

var reply_block = null;
var reply_template = _GO_('rptpl');
var comment_block = _GO_('com_form_field');
if(if_url === undefined) if_url = '/iframe.html';
var $_GU = "//"+_GA_("dm")+if_url;
var $_GH = 0;
var $_GO = null;
$_GU = $_GU.replace(/www\.4399\.com\/iframe/g,"www.4399.com/httpsNot301/iframe");
var _GL_=function(obj,evt,fun){
	if(obj.addEventListener){
		obj.addEventListener(evt,fun,false);
	}
	else if(obj.attachEvent){
		obj.attachEvent("on"+evt, fun);
	}
	else{
		eval("obj.on"+evt+"=fun");
	}
}
var _GR_=function(){
	var h = _GO_('FrameHeight').offsetHeight;
	if(h && h != $_GH){
		//alert(h);
		$_GH = h;
		_GF_(h);
		//alert(h);
	}
}
var _GF_=function(h){
	if(!$_GO){
		$_GO = document.createElement("iframe");
		$_GO.style.display = 'none';
		document.body.appendChild($_GO);
	}
	$_GO.src = $_GU + "#" + h;
}
var _CH_ =function(d){
	var dom = _GO_(d);
	if(dom){
		var dm = _GA_("dm");
		var ls = dom.getElementsByTagName("a");
		for(var i = 0, l = ls.length; i<l; i++){
		    if (ls[i].href.indexOf('void') >= 0) continue;
		    if (ls[i].href.indexOf('dm') >= 0) continue;
		    if (if_url == '/iframe.html')
			    ls[i].href+="?dm="+dm;
            else
			    ls[i].href+="?popup=yes&dm="+dm;
		}
	}
}
var myurl = window.location.href;
if(String(myurl).indexOf('//comment.4399pk.com') < 0){
    _CH_("cppage");
//    _CH_("cptitle");
    setInterval(_GR_, 100);
}
