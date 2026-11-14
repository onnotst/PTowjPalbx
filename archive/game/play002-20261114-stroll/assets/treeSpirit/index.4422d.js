System.register("chunks:///_virtual/FlashDragonBones.ts",["./rollupPluginModLoBabelHelpers.js","cc"],(function(t){var i,e,a,n,r;return{setters:[function(t){i=t.inheritsLoose},function(t){e=t.cclegacy,a=t._decorator,n=t.dragonBones,r=t.Component}],execute:function(){var o;e._RF.push({},"58579oW6O5Cxa7+fp7S6BJ3","FlashDragonBones",void 0);var s=a.ccclass;a.property,t("default",s("FlashDragonBones")(o=function(t){function e(){for(var i,e=arguments.length,a=new Array(e),n=0;n<e;n++)a[n]=arguments[n];return(i=t.call.apply(t,[this].concat(a))||this).duration=.5,i._median=0,i._time=0,i._material=null,i._db=null,i}i(e,t);var a=e.prototype;return a.onLoad=function(){this._median=this.duration/2,this._db=this.node.getComponent(n.ArmatureDisplay),this._material=this._db.customMaterial,this._material.setProperty("u_rate",1)},a.update=function(t){if(this._time>0){this._time-=t,this._time=this._time<0?0:this._time;var i=2*Math.abs(this._time-this._median)/this.duration,e=this._db._materialCache;for(var a in e){e[a].setProperty("u_rate",i)}}},a.clickFlash=function(){this._time=this.duration},e}(r))||o);e._RF.pop()}}}));

System.register("chunks:///_virtual/FlashSpine.ts",["./rollupPluginModLoBabelHelpers.js","cc"],(function(t){var e,i,n,s,a;return{setters:[function(t){e=t.inheritsLoose},function(t){i=t.cclegacy,n=t._decorator,s=t.sp,a=t.Component}],execute:function(){var r;i._RF.push({},"539c1RLUZ1HfYSgRVHurL/Z","FlashSpine",void 0);var o=n.ccclass;n.property,t("default",o("FlashSpine")(r=function(t){function i(){for(var e,i=arguments.length,n=new Array(i),s=0;s<i;s++)n[s]=arguments[s];return(e=t.call.apply(t,[this].concat(n))||this).duration=.5,e._median=0,e._time=0,e._material=null,e._skeleton=null,e}e(i,t);var n=i.prototype;return n.onLoad=function(){this._median=this.duration/2,this._skeleton=this.node.getComponent(s.Skeleton),this._material=this._skeleton.customMaterial,this._material.setProperty("u_rate",1)},n.update=function(t){if(this._time>0){this._time-=t,this._time=this._time<0?0:this._time;var e=2*Math.abs(this._time-this._median)/this.duration,i=this._skeleton._materialCache;for(var n in i){i[n].setProperty("u_rate",e)}}},n.clickFlash=function(){this._time=this.duration},i}(a))||r);i._RF.pop()}}}));

System.register("chunks:///_virtual/FlashSprite.ts",["./rollupPluginModLoBabelHelpers.js","cc"],(function(t){var i,e,r,a,n;return{setters:[function(t){i=t.inheritsLoose},function(t){e=t.cclegacy,r=t._decorator,a=t.Sprite,n=t.Component}],execute:function(){var s;e._RF.push({},"dce509rYeJC0IcpuMndj56O","FlashSprite",void 0);var o=r.ccclass;r.property,t("default",o("FlashSprite")(s=function(t){function e(){for(var i,e=arguments.length,r=new Array(e),a=0;a<e;a++)r[a]=arguments[a];return(i=t.call.apply(t,[this].concat(r))||this).duration=.5,i._median=0,i._time=0,i._material=null,i}i(e,t);var r=e.prototype;return r.onLoad=function(){this._median=this.duration/2,this._material=this.node.getComponent(a).getMaterial(0),this._material.setProperty("u_rate",1)},r.update=function(t){if(this._time>0){this._time-=t,this._time=this._time<0?0:this._time;var i=2*Math.abs(this._time-this._median)/this.duration;this._material.setProperty("u_rate",i)}},r.clickFlash=function(){this._time=this.duration},e}(n))||s);e._RF.pop()}}}));

System.register("chunks:///_virtual/treeSpirit",["./FlashDragonBones.ts","./FlashSpine.ts","./FlashSprite.ts"],(function(){return{setters:[null,null,null],execute:function(){}}}));

(function(r) {
  r('virtual:///prerequisite-imports/treeSpirit', 'chunks:///_virtual/treeSpirit'); 
})(function(mid, cid) {
    System.register(mid, [cid], function (_export, _context) {
    return {
        setters: [function(_m) {
            var _exportObj = {};

            for (var _key in _m) {
              if (_key !== "default" && _key !== "__esModule") _exportObj[_key] = _m[_key];
            }
      
            _export(_exportObj);
        }],
        execute: function () { }
    };
    });
});