﻿#pragma warning(disable : 4244)

#include "framework.h"
#include <filesystem>
#include <fstream>

#include "codec.h"
#include "exec_sql.h"
#include "funcs.h"
#include "log.h"
#include "spy_types.h"
#include "util.h"

#define HEADER_PNG1 0x89
#define HEADER_PNG2 0x50
#define HEADER_JPG1 0xFF
#define HEADER_JPG2 0xD8
#define HEADER_GIF1 0x47
#define HEADER_GIF2 0x49

using namespace std;
namespace fs = std::filesystem;

extern bool gIsListeningPyq;
extern WxCalls_t g_WxCalls;
extern QWORD g_WeChatWinDllAddr;

typedef QWORD (*funcGetSNSDataMgr_t)();
typedef QWORD (*funcGetSnsTimeLineMgr_t)();
typedef QWORD (*funcGetSNSFirstPage_t)(QWORD, QWORD, QWORD);
typedef QWORD (*funcGetSNSNextPageScene_t)(QWORD, QWORD);
typedef QWORD (*GetChatMgr_t)();
typedef QWORD (*NewChatMsg_t)(QWORD);
typedef QWORD (*FreeChatMsg_t)(QWORD);
typedef QWORD (*GetPreDownLoadMgr_t)();
typedef QWORD (*GetMgrByPrefixLocalId_t)(QWORD, QWORD);
typedef QWORD (*PushAttachTask_t)(QWORD, QWORD, QWORD, QWORD);

int IsLogin(void) { return (int)GET_QWORD(g_WeChatWinDllAddr + g_WxCalls.login); }

static string get_key(uint8_t header1, uint8_t header2, uint8_t *key)
{
    // PNG?
    *key = HEADER_PNG1 ^ header1;
    if ((HEADER_PNG2 ^ *key) == header2) {
        return ".png";
    }

    // JPG?
    *key = HEADER_JPG1 ^ header1;
    if ((HEADER_JPG2 ^ *key) == header2) {
        return ".jpg";
    }

    // GIF?
    *key = HEADER_GIF1 ^ header1;
    if ((HEADER_GIF2 ^ *key) == header2) {
        return ".gif";
    }

    return ""; // 错误
}

string DecryptImage(string src, string dir)
{
    if (!fs::exists(src)) {
        LOG_ERROR("File not exists: {}", src);
        return "";
    }

    ifstream in(src.c_str(), ios::binary);
    if (!in.is_open()) {
        LOG_ERROR("Failed to read file {}", src);
        return "";
    }

    filebuf *pfb = in.rdbuf();
    size_t size  = pfb->pubseekoff(0, ios::end, ios::in);
    pfb->pubseekpos(0, ios::in);

    vector<char> buff;
    buff.reserve(size);
    char *pBuf = buff.data();
    pfb->sgetn(pBuf, size);
    in.close();

    uint8_t key = 0x00;
    string ext  = get_key(pBuf[0], pBuf[1], &key);
    if (ext.empty()) {
        LOG_ERROR("Failed to get key.");
        return "";
    }

    for (size_t i = 0; i < size; i++) {
        pBuf[i] ^= key;
    }

    string dst = "";

    try {
        if (dir.empty()) {
            dst = fs::path(src).replace_extension(ext).string();
        } else {
            dst = (dir.back() == '\\' || dir.back() == '/') ? dir : (dir + "/");
            dst += fs::path(src).stem().string() + ext;
        }

        replace(dst.begin(), dst.end(), '\\', '/');
    } catch (const std::exception &e) {
        LOG_ERROR(GB2312ToUtf8(e.what()));
    } catch (...) {
        LOG_ERROR("Unknow exception.");
        return "";
    }

    ofstream out(dst.c_str(), ios::binary);
    if (!out.is_open()) {
        LOG_ERROR("Failed to write file {}", dst);
        return "";
    }

    out.write(pBuf, size);
    out.close();

    return dst;
}

static int GetFirstPage()
{
    int status = -1;

    funcGetSNSDataMgr_t GetSNSDataMgr     = (funcGetSNSDataMgr_t)(g_WeChatWinDllAddr + 0x22A91C0);
    funcGetSNSFirstPage_t GetSNSFirstPage = (funcGetSNSFirstPage_t)(g_WeChatWinDllAddr + 0x2ED9080);

    QWORD buff[16] = { 0 };
    QWORD mgr      = GetSNSDataMgr();
    status         = (int)GetSNSFirstPage(mgr, (QWORD)buff, 1);

    return status;
}

static int GetNextPage(QWORD id)
{
    int status = -1;

    funcGetSnsTimeLineMgr_t GetSnsTimeLineMgr     = (funcGetSnsTimeLineMgr_t)(g_WeChatWinDllAddr + 0x2E6B110);
    funcGetSNSNextPageScene_t GetSNSNextPageScene = (funcGetSNSNextPageScene_t)(g_WeChatWinDllAddr + 0x2EFEC00);

    QWORD mgr = GetSnsTimeLineMgr();
    status    = (int)GetSNSNextPageScene(mgr, id);

    return status;
}

int RefreshPyq(QWORD id)
{
    if (!gIsListeningPyq) {
        LOG_ERROR("没有启动朋友圈消息接收，参考：enable_receiving_msg");
        return -1;
    }

    if (id == 0) {
        return GetFirstPage();
    }

    return GetNextPage(id);
}

/*******************************************************************************
 * 都说我不写注释，写一下吧
 * 其实也没啥好写的，就是下载资源
 * 主要介绍一下几个参数：
 * id：好理解，消息 id
 * thumb：图片或者视频的缩略图路径；如果是视频，后缀为 mp4 后就是存在路径了
 * extra：图片、文件的路径
 *******************************************************************************/
int DownloadAttach(QWORD id, string thumb, string extra)
{
    int status = -1;
    QWORD localId;
    uint32_t dbIdx;

    if (fs::exists(extra)) { // 第一道，不重复下载。TODO: 通过文件大小来判断
        return 0;
    }

    if (GetLocalIdandDbidx(id, &localId, &dbIdx) != 0) {
        LOG_ERROR("Failed to get localId, Please check id: {}", to_string(id));
        return status;
    }

    NewChatMsg_t NewChatMsg                       = (NewChatMsg_t)(g_WeChatWinDllAddr + 0x1C28800);
    FreeChatMsg_t FreeChatMsg                     = (FreeChatMsg_t)(g_WeChatWinDllAddr + 0x1C1FF10);
    GetChatMgr_t GetChatMgr                       = (GetChatMgr_t)(g_WeChatWinDllAddr + 0x1C51CF0);
    GetMgrByPrefixLocalId_t GetMgrByPrefixLocalId = (GetMgrByPrefixLocalId_t)(g_WeChatWinDllAddr + 0x2206280);
    GetPreDownLoadMgr_t GetPreDownLoadMgr         = (GetPreDownLoadMgr_t)(g_WeChatWinDllAddr + 0x1CD87E0);
    PushAttachTask_t PushAttachTask               = (PushAttachTask_t)(g_WeChatWinDllAddr + 0x1DA69C0);

    LARGE_INTEGER l;
    l.HighPart = dbIdx;
    l.LowPart  = (DWORD)localId;

    char *buff = (char *)HeapAlloc(GetProcessHeap(), 0, 0x460);
    if (buff == nullptr) {
        LOG_ERROR("Failed to allocate memory.");
        return status;
    }

    QWORD pChatMsg = NewChatMsg((QWORD)buff);
    GetChatMgr();
    GetMgrByPrefixLocalId(l.QuadPart, pChatMsg);

    QWORD type = GET_QWORD(buff + 0x38);

    string save_path  = "";
    string thumb_path = "";

    switch (type) {
        case 0x03: { // Image: extra
            save_path = extra;
            break;
        }
        case 0x3E:
        case 0x2B: { // Video: thumb
            thumb_path = thumb;
            save_path  = fs::path(thumb).replace_extension("mp4").string();
            break;
        }
        case 0x31: { // File: extra
            save_path = extra;
            break;
        }
        default:
            break;
    }

    if (fs::exists(save_path)) { // 不重复下载。TODO: 通过文件大小来判断
        return 0;
    }

    LOG_DEBUG("path: {}", save_path);
    // 创建父目录，由于路径来源于微信，不做检查
    fs::create_directory(fs::path(save_path).parent_path().string());

    int temp             = 1;
    WxString *pSavePath  = NewWxStringFromStr(save_path);
    WxString *pThumbPath = NewWxStringFromStr(thumb_path);

    memcpy(&buff[0x280], pThumbPath, sizeof(WxString));
    memcpy(&buff[0x2A0], pSavePath, sizeof(WxString));
    memcpy(&buff[0x40C], &temp, sizeof(temp));

    QWORD mgr = GetPreDownLoadMgr();
    status    = (int)PushAttachTask(mgr, pChatMsg, 0, 1);
    FreeChatMsg(pChatMsg);

    return status;
}

#if 0
int RevokeMsg(QWORD id)
{
    int status = -1;
    QWORD localId;
    uint32_t dbIdx;
    if (GetLocalIdandDbidx(id, &localId, &dbIdx) != 0) {
        LOG_ERROR("Failed to get localId, Please check id: {}", to_string(id));
        return status;
    }

    char chat_msg[0x2D8] = { 0 };

    DWORD rmCall1 = g_WeChatWinDllAddr + g_WxCalls.rm.call1;
    DWORD rmCall2 = g_WeChatWinDllAddr + g_WxCalls.rm.call2;
    DWORD rmCall3 = g_WeChatWinDllAddr + g_WxCalls.rm.call3;
    DWORD rmCall4 = g_WeChatWinDllAddr + g_WxCalls.rm.call4;
    DWORD rmCall5 = g_WeChatWinDllAddr + g_WxCalls.rm.call5;

    __asm {
        pushad;
        pushfd;
        lea        ecx, chat_msg;
        call       rmCall1;
        call       rmCall2;
        push       dword ptr [dbIdx];
        lea        ecx, chat_msg;
        push       dword ptr [localId];
        call       rmCall3;
        add        esp, 0x8;
        call       rmCall2;
        lea        ecx, chat_msg;
        push       ecx;
        mov        ecx, eax;
        call       rmCall4;
        mov        status, eax;
        lea        ecx, chat_msg;
        push       0x0;
        call       rmCall5;
        popfd;
        popad;
    }

    return status;
}
#endif

string GetAudio(QWORD id, string dir)
{
    string mp3path = (dir.back() == '\\' || dir.back() == '/') ? dir : (dir + "/");
    mp3path += to_string(id) + ".mp3";
    replace(mp3path.begin(), mp3path.end(), '\\', '/');
    if (fs::exists(mp3path)) { // 不重复下载
        return mp3path;
    }

    vector<uint8_t> silk = GetAudioData(id);
    if (silk.size() == 0) {
        LOG_ERROR("Empty audio data.");
        return "";
    }

    Silk2Mp3(silk, mp3path, 24000);

    return mp3path;
}

#if 0
OcrResult_t GetOcrResult(string path)
{
    OcrResult_t ret = { -1, "" };

    if (!fs::exists(path)) {
        LOG_ERROR("Can not find: {}", path);
        return ret;
    }

    // 路径分隔符有要求，必须为 `\`
    wstring wsPath = String2Wstring(fs::path(path).make_preferred().string());

    WxString wxPath(wsPath);
    WxString nullObj;
    WxString ocrBuffer;

    DWORD ocrCall1 = g_WeChatWinDllAddr + g_WxCalls.ocr.call1;
    DWORD ocrCall2 = g_WeChatWinDllAddr + g_WxCalls.ocr.call2;
    DWORD ocrCall3 = g_WeChatWinDllAddr + g_WxCalls.ocr.call3;

    DWORD tmp  = 0;
    int status = -1;
    __asm {
        pushad;
        pushfd;
        lea   ecx, ocrBuffer;
        call  ocrCall1;
        call  ocrCall2;
        lea   ecx, nullObj;
        push  ecx;
        lea   ecx, tmp;
        push  ecx;
        lea   ecx, ocrBuffer;
        push  ecx;
        push  0x0;
        lea   ecx, wxPath;
        push  ecx;
        mov   ecx, eax;
        call  ocrCall3;
        mov   status, eax;
        popfd;
        popad;
    }

    if (status != 0)
    {
        LOG_ERROR("OCR status: {}", to_string(status));
        return ret; // 识别出错
    }

    ret.status = status;

    DWORD addr   = (DWORD)&ocrBuffer;
    DWORD header = GET_DWORD(addr);
    DWORD num    = GET_DWORD(addr + 0x4);
    if (num <= 0) {
        return ret; // 识别内容为空
    }

    for (uint32_t i = 0; i < num; i++) {
        DWORD content = GET_DWORD(header);
        ret.result += Wstring2String(GET_WSTRING(content + 0x14));
        ret.result += "\n";
        header = content;
    }

    return ret;
}

string GetLoginUrl()
{
    if (GET_DWORD(g_WeChatWinDllAddr + g_WxCalls.login) == 1) {
        LOG_DEBUG("Already logined.");
        return ""; // 已登录直接返回空字符
    }

    DWORD refreshLoginQrcodeCall1 = g_WeChatWinDllAddr + g_WxCalls.rlq.call1;
    DWORD refreshLoginQrcodeCall2 = g_WeChatWinDllAddr + g_WxCalls.rlq.call2;

    // 刷新二维码
    __asm {
        pushad;
        pushfd;
        call refreshLoginQrcodeCall1;
        mov ecx, eax;
        call refreshLoginQrcodeCall2;
        popfd;
        popad;
    }

    // 获取二维码链接
    char *url   = GET_STRING(g_WeChatWinDllAddr + g_WxCalls.rlq.url);
    uint8_t cnt = 0;
    while (url[0] == 0) { // 刷新需要时间，太快了会获取不到
        if (cnt > 5) {
            LOG_ERROR("Refresh QR Code timeout.");
            return "";
        }
        Sleep(1000);
        cnt++;
    }
    return "http://weixin.qq.com/x/" + string(url);
}

int ReceiveTransfer(string wxid, string transferid, string transactionid)
{
    int rv                  = 0;
    DWORD recvTransferCall1 = g_WeChatWinDllAddr + g_WxCalls.tf.call1;
    DWORD recvTransferCall2 = g_WeChatWinDllAddr + g_WxCalls.tf.call2;
    DWORD recvTransferCall3 = g_WeChatWinDllAddr + g_WxCalls.tf.call3;

    char payInfo[0x134] = { 0 };
    wstring wsWxid      = String2Wstring(wxid);
    wstring wsTfid      = String2Wstring(transferid);
    wstring wsTaid      = String2Wstring(transactionid);

    WxString wxWxid(wsWxid);
    WxString wxTfid(wsTfid);
    WxString wxTaid(wsTaid);

    LOG_DEBUG("Receiving transfer, from: {}, transferid: {}, transactionid: {}", wxid, transferid, transactionid);
    __asm {
        pushad;
        lea ecx, payInfo;
        call recvTransferCall1;
        mov dword ptr[payInfo + 0x4], 0x1;
        mov dword ptr[payInfo + 0x4C], 0x1;
        popad;
    }
    memcpy(&payInfo[0x1C], &wxTaid, sizeof(wxTaid));
    memcpy(&payInfo[0x38], &wxTfid, sizeof(wxTfid));

    __asm {
        pushad;
        push 0x1;
        sub esp, 0x8;
        lea edx, wxWxid;
        lea ecx, payInfo;
        call recvTransferCall2;
        mov rv, eax;
        add esp, 0xC;
        push 0x0;
        lea ecx, payInfo;
        call recvTransferCall3;
        popad;
    }

    return rv;
}
#endif
