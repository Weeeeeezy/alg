from os.path import join
import os

import sys
import numpy as np
import pandas as pd
from clickhouse_driver import Client


def load_synchots(ticker, exchange, start_ts, end_ts, folderpath):
    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)

    start_dt_str = int(start_dt.timestamp()*1e9)
    end_dt_str = int(end_dt.timestamp()*1e9)

    settings = {'receive_timeout': 2400}

    client = Client.from_url('clickhouse://tdigital:tdigital2020@click.covexity.com:9000/md5')
    client.settings = settings
    res_sync_orderbook = client.execute(f"""SELECT exch_ts, price, qty, side
                                             from l2ob_BINANCE
                                             where ticker = '{ticker}'
                                             and (u_type = 'syncshot')
                                             and exch_ts >= {start_dt_str} and exch_ts < {end_dt_str}""")

    sync_df = pd.DataFrame(res_sync_orderbook, columns=['exch_ts', 'price', 'qty', 'side'])
    sync_df['timestamp'] = sync_df.exch_ts
    sync_df = sync_df.sort_values('timestamp')
    sync_df['side_int'] = np.where(sync_df.side == 'buy', 1, -1)

    sync_df = sync_df[['timestamp', 'side_int', 'price', 'qty']]
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_syncshots.csv")
    sync_df.to_csv(filepath, index=False, header=False, sep=",")


def load_trades(ticker, exchange, start_ts, end_ts, folderpath):

    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)

    start_dt_str = int(start_dt.timestamp()*1e9)
    end_dt_str = int(end_dt.timestamp()*1e9)

    settings = {'receive_timeout': 2400}
    client = Client.from_url('clickhouse://tdigital:tdigital2020@click.covexity.com:9000/md5')
    client.settings = settings
    res_trades = client.execute(f"""SELECT exch_ts, price, abs(qty) qty, side_int
                                      from taq_BINANCE
                                      where type = 'trade' and ticker = '{ticker}'
                                      and exch_ts >= {start_dt_str} and exch_ts < {end_dt_str}""")

    trades = pd.DataFrame(res_trades, columns=['exch_ts', 'price', 'qty', 'side_int'])
    trades['timestamp'] = trades.exch_ts
    trades = trades.sort_values('timestamp')
    trades = trades[['timestamp', 'side_int', 'price', 'qty']]
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_trades.csv")
    trades.to_csv(filepath, index=False, header=False, sep=",")


def load_updates(ticker, exchange, start_ts, end_ts, folderpath):
    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)

    start_dt_str = int(start_dt.timestamp()*1e9)
    end_dt_str = int(end_dt.timestamp()*1e9)

    settings = {'receive_timeout': 2400}
    client = Client.from_url('clickhouse://tdigital:tdigital2020@click.covexity.com:9000/md5')
    client.settings = settings
    res_orderbook = client.execute(f"""SELECT exch_ts, price, qty, side
                                              from l2ob_BINANCE
                                              where ticker = '{ticker}' and u_type = 'update'
                                              and exch_ts >= {start_dt_str} and exch_ts < {end_dt_str}""")

    upd_df = pd.DataFrame(res_orderbook, columns=['exch_ts', 'price', 'qty', 'side'])
    upd_df['timestamp'] = upd_df.exch_ts
    upd_df = upd_df.sort_values('timestamp')
    upd_df['side_int'] = -1
    upd_df['side_int'] = np.where(upd_df.side == 'buy', 1, -1)
    upd_df = upd_df[['timestamp', 'side_int', 'price', 'qty']]
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_updates.csv")
    upd_df.to_csv(filepath, index=False, header=False, sep=",")


if __name__ == "__main__":
    ticker = sys.argv[1]
    exchange = sys.argv[2]
    start_ts = sys.argv[3]
    end_ts = sys.argv[4]
    folderpath = os.getcwd()



    load_trades(ticker, exchange, start_ts, end_ts, folderpath)
    load_updates(ticker, exchange, start_ts, end_ts, folderpath)
    load_synchots(ticker, exchange, start_ts, end_ts, folderpath)
