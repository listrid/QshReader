#include "./Common/tdFile_WCache.hpp"
#include "./Common/tdZip.h"
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include "QshParser.h"







size_t TimeToStr(int64_t t, char* out32, bool local)
{
    const int64_t unixEpoch = 0x00003883122cd800;
    struct tm timeinfo;
    int64_t rawtime = ((t-unixEpoch)/1000);
    if(local)
    {
        localtime_s(&timeinfo, (time_t*)&rawtime);
    }else{
        gmtime_s(&timeinfo, (time_t*)&rawtime);
    }
    size_t len = strftime(out32, 32, "%d.%m.%Y %H:%M:%S.", &timeinfo);
    len += snprintf(out32+len, 32-len, "%.3u", int((t-unixEpoch)%1000));
    return len;
}


bool writeFlag(tdFile_WCache& writer, bool add, const char* txt, bool add—omma)
{
    if(!add)
        return false;
    if(add—omma)
        writer.Write(", ", 2);
    writer.Write(txt);        
    return true;
}


bool ConvertQSH_to_TXT(QshParser& qsh, const char* fileName)
{
    tdFile file;
    if(!file.Open(fileName, tdFile::crw))
        return false;
    tdFile_WCache writer(0x1000000);
    writer.Init(&file, 0);

    writer.Write(qsh.m_StreamEntry[0].data, qsh.m_StreamEntry[0].len);
    writer.Write("\r\n\r\n", 4);
    if(qsh.m_activStreamType == QshParser_StreamType::StreamType_Deals)
        writer.Write("Received;ExchTime;DealId;Type;Price;Volume;OI\r\n");
    if(qsh.m_activStreamType == QshParser_StreamType::StreamType_AuxInfo)
        writer.Write("Received;ExchTime;Price;AskTotal;BidTotal;OI;HiLimit;LoLimit;Deposit;Rate;Message\r\n");
    if(qsh.m_activStreamType == QshParser_StreamType::StreamType_OrdLog)
        writer.Write("Received;ExchTime;OrderId;Price;Amount;AmountRest;DealId;DealPrice;OI;Flags\r\n");
    char t_str[128];
    while(qsh.Read())
    {
        if(qsh.m_activStreamType == QshParser_StreamType::StreamType_Quotes)
        {
            writer.Write("Received: ", 10);
            writer.Write(t_str, TimeToStr(qsh.m_activDateTime, t_str, true));
            writer.Write("\r\n", 2);

            for(size_t i = 0; i< qsh.m_Quotes.m_count; i++)
            {
                switch(qsh.m_Quotes.m_quote[i].m_type)
                {
                    case QshParser_QuoteType::QuoteType_Ask:writer.Write("Ask;", 4);break;
                    case QshParser_QuoteType::QuoteType_BestAsk:writer.Write("BestAsk;", 8);break;
                    case QshParser_QuoteType::QuoteType_BestBid:writer.Write("BestBid;", 8);break;
                    case QshParser_QuoteType::QuoteType_Bid:writer.Write("Bid;", 4);break;
                    default:writer.Write("Unknown;", 8); break;
                }
                writer.Write(t_str, sprintf(t_str, "%g;%i\r\n", qsh.m_Quotes.m_quote[i].m_price, (int)qsh.m_Quotes.m_quote[i].m_volume));
            }
            continue;
        }
        if(qsh.m_activStreamType == QshParser_StreamType::StreamType_Deals)
        {
            writer.Write(t_str, TimeToStr(qsh.m_activDateTime, t_str, true));
            writer.Write(";", 1);
            writer.Write(t_str, TimeToStr(qsh.m_Deals.m_DateTime, t_str, false));
            writer.Write(";", 1);
            const char* str_type = "Unknown";
            if(qsh.m_Deals.m_Type == QshParser_DealType::DealType_Buy)
                str_type = "Buy";
            if(qsh.m_Deals.m_Type == QshParser_DealType::DealType_Sell)
                str_type = "Sell";
            writer.Write(t_str, sprintf(t_str, "%I64d;%s;%g;", qsh.m_Deals.m_Id, str_type, qsh.m_Deals.m_Price));
            writer.Write(t_str, sprintf(t_str, "%i;%i\r\n", (int)qsh.m_Deals.m_Volume, (int)qsh.m_Deals.m_OI));
            continue;
        }
        if(qsh.m_activStreamType == QshParser_StreamType::StreamType_AuxInfo)
        {
            writer.Write(t_str, TimeToStr(qsh.m_activDateTime, t_str, true));
            writer.Write(";", 1);
            writer.Write(t_str, TimeToStr(qsh.m_AuxInfo.m_DateTime, t_str, false));
            writer.Write(t_str, sprintf(t_str, ";%g;", qsh.m_AuxInfo.m_Price));
            if(qsh.m_AuxInfo.m_AskTotal != 0)
                writer.Write(t_str, sprintf(t_str, "%I64d", qsh.m_AuxInfo.m_AskTotal));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_BidTotal != 0)
                writer.Write(t_str, sprintf(t_str, "%I64d", qsh.m_AuxInfo.m_BidTotal));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_OI != 0)
                writer.Write(t_str, sprintf(t_str, "%I64d", qsh.m_AuxInfo.m_OI));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_HiLimit != 0.)
                writer.Write(t_str, sprintf(t_str, "%g", qsh.m_AuxInfo.m_HiLimit));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_LoLimit != 0.)
                writer.Write(t_str, sprintf(t_str, "%g", qsh.m_AuxInfo.m_LoLimit));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_Deposit != 0.)
                writer.Write(t_str, sprintf(t_str, "%g", qsh.m_AuxInfo.m_Deposit));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_Rate != 0.)
                writer.Write(t_str, sprintf(t_str, "%g", qsh.m_AuxInfo.m_Rate));
            writer.Write(";", 1);
            if(qsh.m_AuxInfo.m_Message.len)
                writer.Write(qsh.m_AuxInfo.m_Message.data, qsh.m_AuxInfo.m_Message.len);
            writer.Write("\r\n", 2);
            continue;
        }
        if(qsh.m_activStreamType == QshParser_StreamType::StreamType_OrdLog)
        {
            writer.Write(t_str, TimeToStr(qsh.m_activDateTime, t_str, true));
            writer.Write(";", 1);
            writer.Write(t_str, TimeToStr(qsh.m_OrdLog.m_DateTime, t_str, false));
            writer.Write(t_str, sprintf(t_str, ";%I64d;%g;%I64d;%I64d;", qsh.m_OrdLog.m_OrderId, qsh.m_OrdLog.m_Price, qsh.m_OrdLog.m_Amount, qsh.m_OrdLog.m_AmountRest));
            writer.Write(t_str, sprintf(t_str, "%I64d;%g;%I64d;", qsh.m_OrdLog.m_DealId, qsh.m_OrdLog.m_DealPrice, qsh.m_OrdLog.m_OI));
            bool addF = false;
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_NonZeroReplAct) != 0, "NonZeroReplAct", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_FlowStart) != 0, "FlowStart", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Add) != 0,  "Add",  addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Fill) != 0, "Fill", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Buy) != 0,  "Buy",  addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Sell) != 0, "Sell", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Snapshot) != 0,  "Snapshot",  addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Quote) != 0,     "Quote",     addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Counter) != 0,   "Counter",   addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_NonSystem) != 0, "NonSystem", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_EndOfTransaction) != 0, "EndOfTransaction", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_FillOrKill) != 0,    "FillOrKill",    addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Moved) != 0,         "Moved",         addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_Canceled) != 0,      "Canceled",      addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_CanceledGroup) != 0, "CanceledGroup", addF);
            addF |= writeFlag(writer, (qsh.m_OrdLog.m_Flags & QshParser_OrdLogFlags::OrdLogFlags_CrossTrade) != 0,    "CrossTrade",    addF);
            writer.Write("\r\n", 2);
            continue;
        }
    }
    writer.Close();
    file.Close();
    return true;
}



void testZip()
{
    tdZipReader zip;
    if(!zip.Open("../create_zip/SBER.zip"))
    {
        printf("Error Open zip file\n");
        getchar();
    }
    QshParser qsh;
    if(!qsh.Open(&zip, "2020-10-30.Quotes.qsh"))
    {
        printf("Error Open qsh file\n");
        getchar();
    }
    if(!ConvertQSH_to_TXT(qsh, "../tmp.txt"))
    {
        printf("Error Create tmp file\n");
        getchar();
    }
}


void testFile()
{
    QshParser qsh;
    if(!qsh.Open("../TATN-12.14.2014-10-14.OrdLog.qsh"))
    {
        printf("Error Open qsh file\n");
        getchar();
    }
    if(!ConvertQSH_to_TXT(qsh, "../tmp.txt"))
    {
        printf("Error Create tmp file\n");
        getchar();
    }
}





int _tmain(int argc, _TCHAR* argv[])
{

    //testZip();
    testFile();
    //getchar();
    return 0;
}




