$(document).ready(function(){
	var defScreenHeight=1000,
	lazyImgName="lzimg_",
	lazyImgArray=[],
	get_offset = function(o){
		var offset={"left":o.offsetLeft, "top":o.offsetTop};
		while(o=o.offsetParent){
			offset.left += o.offsetLeft;
			offset.top += o.offsetTop;
		}
		return offset;
	},
	get_img_position = function(o){
		if(!o) return {"realOffset":0, "correctOffset":0};
		var offset=get_offset(o), real=parseInt(offset.top), currect=parseInt(real - defScreenHeight*0.1);
		return {"realOffset":real, "correctOffset":currect};
	},
    get_screen_height2 = function(){
    	if(window.self&&self.innerHeight) return self.innerHeight;
    	if(document.documentElement&&document.documentElement.clientHeight) return document.documentElement.clientHeight;
    	return document.body.clientHeight;
    },
    get_scroll_top2 = function(){
    	if(typeof(window.pageYOffset) != "undefined") return window.pageYOffset;
    	if(document.documentElement && document.documentElement.scrollTop) return document.documentElement.scrollTop;
    	if(document.body) return document.body.scrollTop;
    },
	fill_array = function(){
		var i=1;
		while(true){
			var lazyImgNodes=document.getElementsByName(lazyImgName + i);
			if(lazyImgNodes && lazyImgNodes.length > 0){
				var s = get_img_position(lazyImgNodes[0]);
				lazyImgArray.push([lazyImgNodes, lazyImgNodes[0], s]);
				i++;
			}
			else break;
		}
	}(),
	load_img = function(o){
		if(!o) return;
		for(var i=0;i<o.length;i++){
			if(o[i].getAttribute("lz_src")){
				o[i].setAttribute("src",o[i].getAttribute("lz_src"));
				o[i].removeAttribute("lz_src", 0);
			}
		}
	},
	load_all_img = function(){
		var i=0;
		while(lazyImgArray[i]){
			load_img(lazyImgArray[i][0]);
			i++;
		}
	},
	remove_scroll_event = function(){
		$(window).unbind("scroll", scroll_handler);
		$(window).unbind("resize", scroll_handler);
	},
	scroll_handler = function(){
	    var t_url = self.location.href;
	    var screenH = 600,scrollT = 0,iframeH = 1026;
	    if (t_url.indexOf('#') > 0){
            var t_para = t_url.substring(t_url.indexOf('#')+1)
            var paras = t_para.split('|');
            screenH = parseInt(paras[0]);
            scrollT = parseInt(paras[1]);
        }else{
            iframeH = 0;
            screenH = parseInt(get_screen_height2());
            scrollT = parseInt(get_scroll_top2());
        }

		if(screenH == 0){
			load_all_img();
			clearInterval(scroll_handler);
			//remove_scroll_event();
		}
		var i=0;
		var j=0;
		while(lazyImgArray[i]){
		    var img_top = parseInt(lazyImgArray[i][2].correctOffset)+iframeH;
		    var s_top = parseInt(screenH)+parseInt(scrollT);
			if(s_top >= img_top){
				load_img(lazyImgArray[i][0]);
				j++
			}
			i++;
			//if(j >=lazyImgArray.length) remove_scroll_event();
			if(j >=lazyImgArray.length) clearInterval(scroll_handler);
		}
	};
	setInterval(scroll_handler, 100);

	//$(window).bind("scroll", scroll_handler);$(window).bind("resize", scroll_handler);scroll_handler();
});
