package app

import (
	"github.com/gin-gonic/gin"
	"net/http"
	"strings"
)

type Result struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data"`
}

// SetMessageCallbackUrl 设置消息回调地址
func SetMessageCallbackUrl(c *gin.Context) {
	var result Result
	var RequestData struct {
		CallbackUrl string `json:"callback_url"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "回调url设置成功"
	var data = map[string]string{"callback_url": RequestData.CallbackUrl}
	result.Data = data
	WxClient.MessageCallbackUrl = RequestData.CallbackUrl
	c.JSON(http.StatusOK, result)
}

// GetSelfWXID 获取登录者的wxid
func GetSelfWXID(c *gin.Context) {
	var result Result
	wxId := WxClient.GetSelfWXID()
	if wxId == "" {
		result.Code = 0
		result.Message = "获取登录者的wx_id失败"
		var data = map[string]string{"wx_id": wxId}
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "获取登录者的wx_id成功"
	var data = map[string]string{"wx_id": wxId}
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// GetUserInfo 获取自己的信息
func GetUserInfo(c *gin.Context) {
	var result Result
	result.Code = 1
	result.Message = "获取个人信息成功"
	result.Data = WxClient.GetUserInfo()
	c.JSON(http.StatusOK, result)
}

// GetMsgTypes 获取消息类型列表
func GetMsgTypes(c *gin.Context) {
	var result Result
	result.Code = 1
	result.Message = "获取消息类型列表成功"
	result.Data = WxClient.GetMsgTypes()
	c.JSON(http.StatusOK, result)
}

// GetContacts 获取通讯录
func GetContacts(c *gin.Context) {
	var result Result
	// 此处手动修改了wcf.pd.go文件 原文件为json字段为空时不返回 如有需要可以自行补上， omitempty
	result.Code = 1
	result.Message = "获取通讯录成功"
	result.Data = WxClient.GetContacts()
	c.JSON(http.StatusOK, result)
}

// GetRoomMembersAll 获取全部群的群成员
func GetRoomMembersAll(c *gin.Context) {
	var result Result
	var RoomMemberList = make(map[string]string)
	contacts := WxClient.ExecDBQuery("MicroMsg.db", "SELECT UserName, NickName FROM Contact;")
	for _, v := range contacts {
		RoomMemberList[string(v.GetFields()[0].Content)] = string(v.GetFields()[1].Content)
	}
	result.Code = 1
	result.Message = "获取全部数据成功"
	result.Data = RoomMemberList
	c.JSON(http.StatusOK, result)
}

type RoomMemberList struct {
	Wxid string `json:"wxid"`
	Name string `json:"name"`
}

// GetRoomMember 获取指定群成员
func GetRoomMember(c *gin.Context) {
	var result Result
	var roomId []string
	var ContactList = make(map[string]string)
	var ChatRoomList = make(map[string]string)
	var RoomMemberResult []*RoomMemberList
	var RequestData struct {
		RoomId string `json:"room_id"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	contacts := WxClient.ExecDBQuery("MicroMsg.db", "SELECT UserName, NickName FROM Contact;")
	for _, v := range contacts {
		ContactList[string(v.GetFields()[0].Content)] = string(v.GetFields()[1].Content)
	}
	roomMembers := WxClient.ExecDBQuery("MicroMsg.db", "SELECT UserNameList FROM ChatRoom WHERE ChatRoomName = '"+RequestData.RoomId+"';")
	for _, v := range roomMembers {
		ChatRoomList["UserNameList"] = string(v.GetFields()[0].Content)
	}
	for _, ChatRoomvalue := range ChatRoomList {
		resultS := strings.Split(ChatRoomvalue, "^G")
		for _, value := range resultS {
			roomId = append(roomId, value)
		}
	}
	for _, value := range roomId {
		for Contactkey, Contactvalue := range ContactList {
			if value == Contactkey {
				roomMemberList := &RoomMemberList{
					Wxid: Contactkey,
					Name: Contactvalue,
				}
				RoomMemberResult = append(RoomMemberResult, roomMemberList)
			}
		}
	}
	result.Code = 1
	result.Message = "获取指定群成员成功"
	result.Data = RoomMemberResult
	c.JSON(http.StatusOK, result)
}

// GetDBNames 获取全部的数据库
func GetDBNames(c *gin.Context) {
	var result Result
	// 此处手动修改了wcf.pd.go文件 原文件为json字段为空时不返回 如有需要可以自行补上， omitempty
	result.Code = 1
	result.Message = "获取全部的数据库成功"
	result.Data = WxClient.GetDBNames()
	c.JSON(http.StatusOK, result)
}

// GetDBTables 获取表
func GetDBTables(c *gin.Context) {
	var result Result
	var RequestData struct {
		DbName string `json:"db_name"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "获取成功"
	result.Data = WxClient.GetDBTables(RequestData.DbName)
	c.JSON(http.StatusOK, result)
}

// ExecDBQuery 执行sql
func ExecDBQuery(c *gin.Context) {
	var result Result
	var RequestData struct {
		Db  string `json:"db"`
		Sql string `json:"sql"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "获取成功"
	var data = WxClient.ExecDBQuery(RequestData.Db, RequestData.Sql)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendTxt 发送文本内容
func SendTxt(c *gin.Context) {
	var result Result
	var RequestData struct {
		Msg      string   `json:"msg"`
		Receiver string   `json:"receiver"`
		Ates     []string `json:"ates"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendTxt(RequestData.Msg, RequestData.Receiver, RequestData.Ates)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendIMG 发送图片
func SendIMG(c *gin.Context) {
	var result Result
	var RequestData struct {
		Path     string `json:"path"`
		Receiver string `json:"receiver"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendIMG(RequestData.Path, RequestData.Receiver)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendFile 发送文件
func SendFile(c *gin.Context) {
	var result Result
	var RequestData struct {
		Path     string `json:"path"`
		Receiver string `json:"receiver"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendFile(RequestData.Path, RequestData.Receiver)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendRichText 发送卡片消息
func SendRichText(c *gin.Context) {
	var result Result
	var RequestData struct {
		Name     string `json:"name"`
		Account  string `json:"account"`
		Title    string `json:"title"`
		Digest   string `json:"digest"`
		Url      string `json:"url"`
		ThumbUrl string `json:"thumb_url"`
		Receiver string `json:"receiver"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendRichText(RequestData.Name, RequestData.Account, RequestData.Title, RequestData.Digest, RequestData.Url, RequestData.ThumbUrl, RequestData.Receiver)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendPat 发送拍一拍消息
func SendPat(c *gin.Context) {
	var result Result
	var RequestData struct {
		RoomId string `json:"room_id"`
		WxId   string `json:"wx_id"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendPat(RequestData.RoomId, RequestData.WxId)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// ForwardMsg 发送拍一拍消息
func ForwardMsg(c *gin.Context) {
	var result Result
	var RequestData struct {
		Id       uint64 `json:"id"`
		Receiver string `json:"receiver"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "转发完成"
	var data = WxClient.ForwardMsg(RequestData.Id, RequestData.Receiver)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// SendEmotion 发送gif
func SendEmotion(c *gin.Context) {
	var result Result
	var RequestData struct {
		Path     string `json:"path"`
		Receiver string `json:"receiver"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.SendEmotion(RequestData.Path, RequestData.Receiver)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// AcceptFriend 接受好友请求
func AcceptFriend(c *gin.Context) {
	var result Result
	var RequestData struct {
		V3    string `json:"v3"`
		V4    string `json:"v4"`
		Scene int32  `json:"scene"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "接收成功"
	var data = WxClient.AcceptFriend(RequestData.V3, RequestData.V4, RequestData.Scene)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// AddChatroomMembers 添加群成员
func AddChatroomMembers(c *gin.Context) {
	var result Result
	var RequestData struct {
		RoomId string   `json:"room_id"`
		WxId   []string `json:"wx_ids"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.AddChatRoomMembers(RequestData.RoomId, RequestData.WxId)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// DelChatRoomMembers 添加群成员
func DelChatRoomMembers(c *gin.Context) {
	var result Result
	var RequestData struct {
		RoomId string   `json:"room_id"`
		WxId   []string `json:"wx_ids"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.DelChatRoomMembers(RequestData.RoomId, RequestData.WxId)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// InvChatRoomMembers 邀请群成员
func InvChatRoomMembers(c *gin.Context) {
	var result Result
	var RequestData struct {
		RoomId string   `json:"room_id"`
		WxId   []string `json:"wx_ids"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.InvChatRoomMembers(RequestData.RoomId, RequestData.WxId)
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// RefreshPyq 刷新朋友圈
func RefreshPyq(c *gin.Context) {
	var result Result
	result.Code = 1
	result.Message = "发送完成"
	var data = WxClient.RefreshPYQ()
	result.Data = data
	c.JSON(http.StatusOK, result)
}

// DownloadAttach 下载附件
func DownloadAttach(c *gin.Context) {
	var result Result
	var RequestData struct {
		Id    uint64 `json:"id"`
		Thumb string `json:"thumb"`
		Extra string `json:"extra"`
	}
	if err := c.BindJSON(&RequestData); err != nil {
		result.Code = 0
		result.Message = "json解析失败"
		var data = make(map[string]interface{})
		result.Data = data
		c.JSON(http.StatusOK, result)
		return
	}
	result.Code = 1
	result.Message = "下载附件调用成功"
	var data = WxClient.DownloadAttach(RequestData.Id, RequestData.Thumb, RequestData.Extra)
	result.Data = data
	c.JSON(http.StatusOK, result)
}
