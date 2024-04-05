import sqlite3

CHUNK_FIELDS = ["ply", "depth", "alpha", "beta", "isPV", "score", "fen",
                "best_move", "move", "static_eval", "razoring", "futility", "nodes_searched", "improving", "cache_move",
                "cache_depth", "cache_score", "cache_flag",
                "move_order", "pv_list"]


class Database:
    def __init__(self, filepath):
        self.__conn = sqlite3.connect(filepath)
        self.__cursor = self.__conn.cursor()

    def create_tables(self):
        with self.__conn:
            self.__conn.execute('''
                      CREATE TABLE IF NOT EXISTS chunks
                      ([id] INTEGER PRIMARY KEY,
                       [ply] INTEGER,
                       [depth] INTEGER,
                       [alpha] INTEGER,
                       [beta] INTEGER,
                       [isPV] INTEGER,
                       [score] INTEGER,
                       [fen] TEXT(90),
                       [move] TEXT(5),
                       [best_move] TEXT(5),
                       [static_eval] INTEGER,
                       [razoring] INTEGER,
                       [futility] INTEGER,
                       [nodes_searched] INTEGER,
                       [improving] INTEGER,
                       [cache_move] TEXT(5),
                       [cache_depth] INTEGER,
                       [cache_score] INTEGER,
                       [cache_flag] INTEGER,
                       [move_order] TEXT,
                       [pv_list] TEXT)
                      ''')

            self.__conn.execute('''
                      CREATE TABLE IF NOT EXISTS parents
                      ([parent_id] INTEGER,
                       [child_id] INTEGER)
                    ''')

    def get_conn(self):
        return self.__conn

    def insert_chunk(self, chunk):
        fields = ",".join(CHUNK_FIELDS)
        values = ",".join(map(lambda _: "?", CHUNK_FIELDS))
        self.__cursor.execute(f"INSERT INTO chunks({fields}) VALUES({values})",
                              [chunk[field] for field in CHUNK_FIELDS])
        id_ = self.__cursor.lastrowid
        return id_

    def insert_connections(self, parent_id, child_ids):
        for child_id in child_ids:
            self.__cursor.execute("INSERT INTO parents(parent_id, child_id) VALUES(?,?)",
                                  (parent_id, child_id))

    def select_chunk(self, id_):
        fields = ",".join(CHUNK_FIELDS)
        with self.__conn:
            result = self.__conn.execute(
                f"SELECT {fields} FROM chunks WHERE id = :id;", {"id": id_})
        return dict(zip(CHUNK_FIELDS, result.fetchone()))

    def select_child_ids(self, parent_id):
        with self.__conn:
            if parent_id is not None:
                result = self.__conn.execute(
                    f"SELECT child_id FROM parents WHERE parent_id={parent_id}")
            else:
                result = self.__conn.execute(
                    "SELECT child_id FROM parents WHERE parent_id is NULL")
        return list(result.fetchall())

    def get_root_id(self):
        with self.__conn:
            result = self.__conn.execute(
                "SELECT id FROM chunks WHERE depth=-1")
        return result.fetchone()[0]
