#pragma once

class QshReader;
class tdZipReader;

#define QSH_GZIP_TEST_CRC32

class QshParser_Quotes;
class QshParser_Deals;
class QshParser_OwnOrders;
class QshParser_OwnTrades;
class QshParser_AuxInfo;
class QshParser_OrdLog;

enum QshParser_StreamType
{
    StreamType_Unknown   = 0x00,
    StreamType_Quotes    = 0x10,
    StreamType_Deals     = 0x20,
    StreamType_OwnOrders = 0x30,
    StreamType_OwnTrades = 0x40,
    StreamType_Messages  = 0x50,
    StreamType_AuxInfo   = 0x60,
    StreamType_OrdLog    = 0x70
};


enum QshParser_QuoteType { QuoteType_Unknown = 0, QuoteType_Ask, QuoteType_Bid, QuoteType_BestAsk, QuoteType_BestBid };
enum QshParser_DealType  { DealType_Unknown = 0, DealType_Buy, DealType_Sell };
enum QshParser_OwnOrderType
{
    OwnOrderType_None = 0, // ��� ������ ���� �����
    OwnOrderType_Regular,  //
    OwnOrderType_Stop      // �����
};
enum QshParser_MessageType { MessageType_None = 0, MessageType_Info, MessageType_Warning, MessageType_Error };


enum QshParser_OrdLogFlags
{
    OrdLogFlags_NonZeroReplAct = 0x0001,// ��� ��������� ������ ������ ���� ReplAct �� ���� ����� ����
    OrdLogFlags_FlowStart  = 0x0002,    // ������ ������ �������� � ����� ��������������� ������ ��� ����� ��������� ����� ������ ����� ������
    OrdLogFlags_Add        = 0x0004,    // ����� ������
    OrdLogFlags_Fill       = 0x0008,    // ������ ������� � ������

    OrdLogFlags_Buy        = 0x0010,    // �������
    OrdLogFlags_Sell       = 0x0020,    // �������

    OrdLogFlags_Snapshot   = 0x0040,    // ������ �������� �� ������ �������� �������

    OrdLogFlags_Quote      = 0x0080,    // ������������
    OrdLogFlags_Counter    = 0x0100,    // ���������
    OrdLogFlags_NonSystem  = 0x0200,    // ������������
    OrdLogFlags_EndOfTransaction = 0x0400, // ������ �������� ��������� � ����������
    OrdLogFlags_FillOrKill = 0x0800,    // ������ Fill-or-kill
    OrdLogFlags_Moved      = 0x1000,    // ������ �������� ����������� �������� ����������� ������
    OrdLogFlags_Canceled   = 0x2000,    // ������ �������� ����������� �������� �������� ������
    OrdLogFlags_CanceledGroup = 0x4000, // ������ �������� ����������� ���������� ��������
    OrdLogFlags_CrossTrade = 0x8000     // ������� �������� ������� ������ �� ������� �����-������
};


struct qsh_str
{
    size_t len;
    char* data;
    void Clear() { len = 0, data = NULL; }
};


class QshParser
{
public:
    struct Quote
    {
        double  m_price;   // ����
        int64_t m_volume;  // �����
        QshParser_QuoteType m_type;
    };
    struct Quotes
    {
        double m_tick;   // ��� ����

        size_t m_count;  // ���������� ��������� � �������
        Quote* m_quote;
        size_t m_countMax; // (���������� �������������)
    };
    struct Deals
    {
        double  m_tick;     // ��� ����

        QshParser_DealType m_Type; //
        int64_t m_DateTime; // �������� ���� � ����� ������
        int64_t m_Id;       // ����� ������ � �������� �������
        int64_t m_OrderId;  // ����� ������, �� ������� ��������� ������ ������
        double  m_Price;    // ���� ������ 
        int64_t m_Volume;   // ����� ������
        int64_t m_OI;       // �������� ������� �� ����������� ����� ���������� ������
    };
    struct OwnOrders
    {
        double  m_tick;     // ��� ����

        QshParser_OwnOrderType m_Type;
        int64_t m_Id;       // ����� ������
        double  m_Price;    // ����
        int64_t m_Quantity; // ������� � ������ (������������� ��� �������, ������������� ��� �������)
    };
    struct OwnTrades
    {
        double  m_tick;

        int64_t m_DateTime; // �������� ����� ������
        int64_t m_TradeId;  // ����� ������ � �������� �������
        int64_t m_OrderId;  // ����� ������, �� ������� ��������� ������ ������
        double  m_Price;    // ���� ������
        int64_t m_Quantity; // ����� ������ (������������� ��� �������, ������������� ��� �������)
    };
    struct Message
    {
        int64_t m_DateTime;           // ��������� ����� ���������
        qsh_str m_Text;               // ����� ��������� (!!! ��� ������������ '\0' )
        QshParser_MessageType m_Type; // ��� ��������� (��������������, ��������������, ������)
    };
    struct AuxInfo
    {
        double  m_tick;

        int64_t m_DateTime; // �������� ����� ���������� ������
        double  m_Price;    // ���� ��������� ������ �� �����������
        int64_t m_AskTotal; // ��������� ����� ��������� "ask"
        int64_t m_BidTotal; // ��������� ����� ��������� "bid"
        int64_t m_OI;       // ���������� �������� ������� (�������� �������)
        double  m_HiLimit;  // ������� ����� ����
        double  m_LoLimit;  // ������ ����� ����
        double  m_Deposit;  // ����������� ����������� � �������� ��������
        double  m_Rate;     // ���� ��������� ������� ����������� � �������� �������
        qsh_str m_Message;  // ��������� �������� �������
    };
    struct OrdLog
    {
        double  m_tick;
        QshParser_OrdLogFlags m_Flags;
        int64_t m_DateTime;   // �������� �����
        int64_t m_OrderId;    // �������� ����� ������
        double  m_Price;      // ���� � ������
        int64_t m_Amount;     // ����� ��������
        //����������� ������ ��� ������� � ������ OrdLogFlags_Fill
        int64_t m_AmountRest; // ������� � ������ ����� ��������
        int64_t m_DealId;     // ����� ������, � ������� ���� ������� ������
        double  m_DealPrice;  // ���� ������
        int64_t m_OI;         // �������� ��������� �������� ����� ������
    };
private:
    QshReader* m_reader;

    void ReadNextRecordHeader();
    bool ReadQuotes();
    bool ReadDeals();
    bool ReadOwnOrders();
    bool ReadOwnTrades();
    bool ReadMessages();
    bool ReadAuxInfo();
    bool ReadOrdLog();

    bool m_initReadHeader;
    union
    {
        QshParser_Quotes* m_QuotesParser;
        QshParser_Deals*  m_DealsParser;
        QshParser_OwnOrders* m_OwnOrdersParser;
        QshParser_OwnTrades* m_OwnTradesParser;
        QshParser_AuxInfo*   m_AuxInfoParser;
        QshParser_OrdLog*    m_OrdLogParser;
    }m_StreamParser[8];
    bool OpenEx();
public:

    QshParser();
    ~QshParser();
    bool Open(const char* fileName);
    bool Open(tdZipReader* zip, const char* fileName);

    void Close();
    bool Read();


    qsh_str m_AppName;     // ��� ����������, � ������� �������� ������� ����
    qsh_str m_Comment;     // ������������ ���������������� �����������
    int64_t m_RecDateTime; // ���� � ����� ������ ������ ����� (UTC)

    size_t  m_StreamCount;    // ������� ������� (�������������� �� 8)
    qsh_str m_StreamEntry[8]; // ������ ��� �����������, �������� ������������� �����
    QshParser_StreamType m_StreamType[8];  // ������������� ������


    size_t  m_activStream; // ������� ������������ �����
    int64_t m_activDateTime;
    QshParser_StreamType m_activStreamType;

// ������������� ������� �� m_activStreamType
    Quotes    m_Quotes;    // ������ (StreamType_Quotes)
    Deals     m_Deals;     // ���� ������ (StreamType_Deals)
    OwnOrders m_OwnOrders; // c��� ������ (StreamType_OwnOrders)
    OwnTrades m_OwnTrades; // c��� ������ (StreamType_OwnTrades)
    Message   m_Message;   // ��������� (StreamType_Messages)
    AuxInfo   m_AuxInfo;   // ��������������� ������ (StreamType_AuxInfo)
    OrdLog    m_OrdLog;    // ������ ������ ������ (QshParser_OrdLog)
};

