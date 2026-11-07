/**
 * CoordConvert.js
 * 坐标系转换工具：WGS-84 / GCJ-02 / BD-09
 * 支持浏览器原生 / ES Module / CommonJS
 */
(function (root, factory) {
  if (typeof module === 'object' && typeof module.exports === 'object') {
    // CommonJS
    module.exports = factory();
  } else if (typeof define === 'function' && define.amd) {
    // AMD
    define([], factory);
  } else {
    // 浏览器全局
    root.CoordConvert = factory();
  }
})(typeof self !== 'undefined' ? self : this, function () {

  'use strict';

  var PI = Math.PI;

  /* ==================== 工具函数 ==================== */

  function outOfChina(lat, lng) {
    return !(lng > 73.66 && lng < 135.05 && lat > 3.86 && lat < 53.55);
  }

  function transformLat(x, y) {
    var ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * Math.sqrt(Math.abs(x));
    ret += (20.0 * Math.sin(6.0 * x * PI) + 20.0 * Math.sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * Math.sin(y * PI) + 40.0 * Math.sin(y / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * Math.sin(y / 12.0 * PI) + 320 * Math.sin(y * PI / 30.0)) * 2.0 / 3.0;
    return ret;
  }

  function transformLng(x, y) {
    var ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * Math.sqrt(Math.abs(x));
    ret += (20.0 * Math.sin(6.0 * x * PI) + 20.0 * Math.sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * Math.sin(x * PI) + 40.0 * Math.sin(x / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * Math.sin(x / 12.0 * PI) + 300.0 * Math.sin(x / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
  }

  /* ==================== 核心转换 ==================== */

  function wgs84ToGcj02(lat, lng) {
    if (outOfChina(lat, lng)) return [lat, lng];

    var dLat = transformLat(lng - 105.0, lat - 35.0);
    var dLng = transformLng(lng - 105.0, lat - 35.0);

    var radLat = lat / 180.0 * PI;
    var magic = Math.sin(radLat);
    magic = 1 - 0.00669342162296594323 * magic * magic;
    var sqrtMagic = Math.sqrt(magic);

    dLat = (dLat * 180.0) / (6378245.0 * (1 - 0.00669342162296594323) / (magic * sqrtMagic) * PI);
    dLng = (dLng * 180.0) / (6378245.0 / sqrtMagic * Math.cos(radLat) * PI);

    return [lat + dLat, lng + dLng];
  }

  function gcj02ToWgs84(lat, lng) {
    if (outOfChina(lat, lng)) return [lat, lng];

    var initDelta = 0.01;
    var threshold = 0.000001;
    var dLat = initDelta, dLng = initDelta;
    var mLat = lat - dLat, mLng = lng - dLng;
    var pLat = lat + dLat, pLng = lng + dLng;
    var wgsLat, wgsLng;

    for (var i = 0; i < 30; i++) {
      wgsLat = (mLat + pLat) / 2;
      wgsLng = (mLng + pLng) / 2;
      var tmp = wgs84ToGcj02(wgsLat, wgsLng);
      dLat = tmp[0] - lat;
      dLng = tmp[1] - lng;
      if (Math.abs(dLat) < threshold && Math.abs(dLng) < threshold) break;
      if (dLat > 0) pLat = wgsLat; else mLat = wgsLat;
      if (dLng > 0) pLng = wgsLng; else mLng = wgsLng;
    }
    return [wgsLat, wgsLng];
  }

  function gcj02ToBd09(lat, lng) {
    var z = Math.sqrt(lng * lng + lat * lat) + 0.00002 * Math.sin(lat * PI * 3000.0 / 180.0);
    var theta = Math.atan2(lat, lng) + 0.000003 * Math.cos(lng * PI * 3000.0 / 180.0);
    return [
      z * Math.sin(theta) + 0.006,
      z * Math.cos(theta) + 0.0065
    ];
  }

  function bd09ToGcj02(lat, lng) {
    var x = lng - 0.0065;
    var y = lat - 0.006;
    var z = Math.sqrt(x * x + y * y) - 0.00002 * Math.sin(y * PI * 3000.0 / 180.0);
    var theta = Math.atan2(y, x) - 0.000003 * Math.cos(x * PI * 3000.0 / 180.0);
    return [
      z * Math.sin(theta),
      z * Math.cos(theta)
    ];
  }

  /* ==================== 对外统一接口 ==================== */

  function convert(lat, lng, from, to) {
    from = (from || 'wgs84').toLowerCase();
    to = (to || 'gcj02').toLowerCase();

    if (from === to) return [lat, lng];

    if (from === 'wgs84') [lat, lng] = wgs84ToGcj02(lat, lng);
    else if (from === 'bd09') [lat, lng] = bd09ToGcj02(lat, lng);
    else if (from !== 'gcj02') throw new Error('不支持的源坐标系: ' + from);

    if (to === 'wgs84') return gcj02ToWgs84(lat, lng);
    if (to === 'bd09') return gcj02ToBd09(lat, lng);
    if (to !== 'gcj02') throw new Error('不支持的目标坐标系: ' + to);

    return [lat, lng];
  }

  function convertToObject(lat, lng, from, to) {
    var result = convert(lat, lng, from, to);
    return {
      original: { lat: lat, lng: lng, type: typeName(from) },
      converted: { lat: result[0], lng: result[1], type: typeName(to) },
      from: from,
      to: to
    };
  }

  function typeName(code) {
    return ({
      wgs84: 'WGS-84',
      gcj02: 'GCJ-02',
      bd09: 'BD-09'
    })[code] || code;
  }

  /* ==================== 暴露 API ==================== */

  return {
    convert: convert,
    convertToObject: convertToObject,
    wgs84ToGcj02: wgs84ToGcj02,
    gcj02ToWgs84: gcj02ToWgs84,
    gcj02ToBd09: gcj02ToBd09,
    bd09ToGcj02: bd09ToGcj02,
    typeName: typeName
  };
});