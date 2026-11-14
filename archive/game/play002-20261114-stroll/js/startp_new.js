function stars_4399(id){				//返回对象
	return document.getElementById(id);
}
function star(){
	for (var i=1;i<=5;++i){
		stars_4399('stars_'+i).onclick 		= function(){stars_set('stars_',this); return false;}
		stars_4399('stars_'+i).onmouseover 	= function(){star_over('stars_',this); return false;}
		stars_4399('stars_'+i).onmouseout 	= function(){star_out('stars_',this); return false;}
	}
}
var star_holder; 	// 鼠标停留的评分控件
var star_pre_set; 	// 保存了评分值（通过单击来进行评分）
var star_rated;  	// 是否评分过，并保存了结果（赋值后，就不能再评分）
/**
	点击进行评分
**/
function stars_set(star_id, star_obj){
	var star_num = parseInt(star_obj.id.replace(star_id, ''));
	if(!star_rated){
		if (star_num == 1){
			stars_4399('star_div_id').innerHTML = ' 我要评分(不好玩)';
		}
		else if (star_num == 2){
			stars_4399('star_div_id').innerHTML = ' 我要评分(较一般)';
		}
		else if (star_num == 3){
			stars_4399('star_div_id').innerHTML = ' 我要评分(有意思)';
		}
		else if (star_num == 4){
			stars_4399('star_div_id').innerHTML = ' 我要评分(很推荐)';
		}
		else{
			stars_4399('star_div_id').innerHTML = ' 我要评分(超好玩)';
		}
		star_pre_set = star_obj;
		star_rated 	 = 0;  //设为1以后，就变成了最终结果，不能再修改评分结果
		document.getElementById("score").value = star_num;

		star_over(star_id, star_obj);
		star_holder = star_num;
	}
}

/**
	鼠标停留事件
**/
function star_over(star_id, star_obj){
	if(!star_rated){
		var star_num = parseInt(star_obj.id.replace(star_id, '')); // 获取选中的星星的索引，这里使用_1,_2,_3,_4,_5来做为评分控件的ID，当然也有其他的方式。
		for(var i=1;i<=5;i++){
			if(i <= star_num){
				stars_4399(star_id+i).className = 's2';
				if (star_num == 1){
					stars_4399('star_div_id').innerHTML = ' 我要评分(不好玩)';
				}
				else if (star_num == 2){
					stars_4399('star_div_id').innerHTML = ' 我要评分(较一般)';
				}
				else if (star_num == 3){
					stars_4399('star_div_id').innerHTML = ' 我要评分(有意思)';
				}
				else if (star_num == 4){
					stars_4399('star_div_id').innerHTML = ' 我要评分(很推荐)';
				}
				else{
					stars_4399('star_div_id').innerHTML = ' 我要评分(超好玩)';
				}
			}
			else{
				stars_4399(star_id+i).className = 's1';
			}
		}
	}
}
/**
	鼠标离开事件
**/
function star_out(star_id,star_obj){
	if(!star_rated){
		if(!star_pre_set){
			for(i=1;i<=5;i++){
				stars_4399(star_id+i).className			= 's1';
				stars_4399('star_div_id').innerHTML 	= ' 我要评分(滑动星星评分)';
			}
		}
		else{
			star_over(star_id,star_pre_set);
		}
	}
}
star();