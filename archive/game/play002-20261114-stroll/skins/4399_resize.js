var Height = 0;
var ResizeFrame=function(){
var _hash = window.location.hash.substring(1);
if (_hash.indexOf('|') > 0){
    var paras = _hash.split('|');
    var _height = paras[0];
}
else
    var _height = _hash;
if(_height != Height){
parent.parent.F_Resize(_height);
Height = _height;
}
setTimeout(ResizeFrame,100);
}
ResizeFrame();
