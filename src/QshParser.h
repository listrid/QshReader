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
    OwnOrderType_None = 0, // все заявки были сняты
    OwnOrderType_Regular,  //
    OwnOrderType_Stop      // снята
};
enum QshParser_MessageType { MessageType_None = 0, MessageType_Info, MessageType_Warning, MessageType_Error };


enum QshParser_OrdLogFlags
{
    OrdLogFlags_NonZeroReplAct = 0x0001,// при получении данной записи поле ReplAct не было равно нулю
    OrdLogFlags_FlowStart  = 0x0002,    // данная запись получена с новым идентификатором сессии или после сообщения смены номера жизни потока
    OrdLogFlags_Add        = 0x0004,    // новая заявка
    OrdLogFlags_Fill       = 0x0008,    // заявка сведена в сделку

    OrdLogFlags_Buy        = 0x0010,    // покупка
    OrdLogFlags_Sell       = 0x0020,    // продажа

    OrdLogFlags_Snapshot   = 0x0040,    // запись получена из архива торговой системы

    OrdLogFlags_Quote      = 0x0080,    // котировочная
    OrdLogFlags_Counter    = 0x0100,    // встречная
    OrdLogFlags_NonSystem  = 0x0200,    // внесистемная
    OrdLogFlags_EndOfTransaction = 0x0400, // запись является последней в транзакции
    OrdLogFlags_FillOrKill = 0x0800,    // заявка Fill-or-kill
    OrdLogFlags_Moved      = 0x1000,    // запись является результатом операции перемещения заявки
    OrdLogFlags_Canceled   = 0x2000,    // запись является результатом операции удаления заявки
    OrdLogFlags_CanceledGroup = 0x4000, // запись является результатом группового удаления
    OrdLogFlags_CrossTrade = 0x8000     // признак удаления остатка заявки по причине кросс-сделки
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
        double  m_price;   // цена
        int64_t m_volume;  // объем
        QshParser_QuoteType m_type;
    };
    struct Quotes
    {
        double m_tick;   // тик цены

        size_t m_count;  // количество котировок в стакане
        Quote* m_quote;
        size_t m_countMax; // (внутреннее использование)
    };
    struct Deals
    {
        double  m_tick;     // тик цены

        QshParser_DealType m_Type; //
        int64_t m_DateTime; // биржевые дата и время сделки
        int64_t m_Id;       // номер сделки в торговой системе
        int64_t m_OrderId;  // номер заявки, по которой совершена данная сделка
        double  m_Price;    // цена сделки 
        int64_t m_Volume;   // объем сделки
        int64_t m_OI;       // открытый интерес по инструменту после совершения сделки
    };
    struct OwnOrders
    {
        double  m_tick;     // тик цены

        QshParser_OwnOrderType m_Type;
        int64_t m_Id;       // номер заявки
        double  m_Price;    // цена
        int64_t m_Quantity; // остаток в заявке (положительный для покупки, отрицательный для продажи)
    };
    struct OwnTrades
    {
        double  m_tick;

        int64_t m_DateTime; // биржевое время сделки
        int64_t m_TradeId;  // номер сделки в торговой системе
        int64_t m_OrderId;  // номер заявки, по которой совершена данная сделка
        double  m_Price;    // цена сделки
        int64_t m_Quantity; // объем сделки (положительный для покупки, отрицательный для продажи)
    };
    struct Message
    {
        int64_t m_DateTime;           // локальное время сообщения
        qsh_str m_Text;               // текст сообщения (!!! без завершающего '\0' )
        QshParser_MessageType m_Type; // тип сообщения (информационное, предупреждение, ошибка)
    };
    struct AuxInfo
    {
        double  m_tick;

        int64_t m_DateTime; // биржевое время обновления данных
        double  m_Price;    // цена последней сделки по инструменту
        int64_t m_AskTotal; // суммарный объем котировок "ask"
        int64_t m_BidTotal; // суммарный объем котировок "bid"
        int64_t m_OI;       // количество открытых позиций (открытый интерес)
        double  m_HiLimit;  // верхний лимит цены
        double  m_LoLimit;  // нижний лимит цены
        double  m_Deposit;  // гарантийное обеспечение в денежных единицах
        double  m_Rate;     // курс пересчета пунктов инструмента в денежные единицы
        qsh_str m_Message;  // сообщение торговой системы
    };
    struct OrdLog
    {
        double  m_tick;
        QshParser_OrdLogFlags m_Flags;
        int64_t m_DateTime;   // биржевое время
        int64_t m_OrderId;    // биржевой номер заявки
        double  m_Price;      // цена в заявке
        int64_t m_Amount;     // объем операции
        //указываются только для записей с флагом OrdLogFlags_Fill
        int64_t m_AmountRest; // остаток в заявке после операции
        int64_t m_DealId;     // номер сделки, в которую была сведена заявка
        double  m_DealPrice;  // цена сделки
        int64_t m_OI;         // значение открытого интереса после сделки
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


    qsh_str m_AppName;     // имя приложения, с помощью которого записан файл
    qsh_str m_Comment;     // произвольный пользовательский комментарий
    int64_t m_RecDateTime; // дата и время начала записи файла (UTC)

    size_t  m_StreamCount;    // сколько стримов (поддерживается до 8)
    qsh_str m_StreamEntry[8]; // полный код инструмента, которому соответствует стрим
    QshParser_StreamType m_StreamType[8];  // идентификатор стрима


    size_t  m_activStream; // текущий распарсенный стрим
    int64_t m_activDateTime;
    QshParser_StreamType m_activStreamType;

// заполненность зависит от m_activStreamType
    Quotes    m_Quotes;    // стакан (StreamType_Quotes)
    Deals     m_Deals;     // тики сделок (StreamType_Deals)
    OwnOrders m_OwnOrders; // cвои заявки (StreamType_OwnOrders)
    OwnTrades m_OwnTrades; // cвои сделки (StreamType_OwnTrades)
    Message   m_Message;   // сообщения (StreamType_Messages)
    AuxInfo   m_AuxInfo;   // вспомогательные данные (StreamType_AuxInfo)
    OrdLog    m_OrdLog;    // полный журнал заявок (QshParser_OrdLog)
};

