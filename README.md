# PTowjPalbx 我的网站
我的网页（纯静态资源）

# 警告
coi-serviceworker.js <!-- 必须在根目录 -->
coi-serviceworker.js <!-- 不得删除，这个用来处理跨域信息 -->
<head>
    <script>
        navigator.serviceWorker.register('/coi-serviceworker.js') <!-- 必须在head的最前面 -->
    </script>
</head>


## 0. 配置你的系统用户信息（单个用户使用）
git config --global user.name "onnotst"
git config --global user.email "onnotst@163.com"
git config --global core.autocrlf true

.gitattributes处理换行问题

## 1. 查看你修改了哪些文件
git status

## 2. 将修改的文件添加到暂存区（. 表示添加所有修改）
git add .
git add --renormalize .

## 3. 提交修改到本地仓库，并写上修改说明
git commit -m "这里写你这次修改的简要说明"

## 4. 推送到 GitHub 远程仓库（如果是第一次推送，可能需要输入账号密码或 Token）
git push origin main