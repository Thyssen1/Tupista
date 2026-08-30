using Microsoft.Data.Sqlite;

namespace Tupista.Storage;

/// <summary>
/// Saved puzzles, in a SQLite file.
///
/// Every method opens and closes its own connection. That looks wasteful and
/// is not: SQLite is a file, not a server, so "connecting" is opening a file
/// handle. Short-lived connections avoid a whole category of bug where a
/// long-lived one is used from two threads at once. (Connection pooling is
/// switched off below so that "closed" really means closed — see Open.)
///
/// All SQL here uses parameters rather than string concatenation. A puzzle name
/// is user input, and user input in a SQL string is how injection happens.
/// </summary>
public sealed class BoardRepository
{
    private readonly string _connectionString;

    /// <summary>
    /// Opens (and creates, if needed) the database at the given path.
    /// Pass a path to control where it lives; the default is per-user.
    /// </summary>
    public BoardRepository(string? databasePath = null)
    {
        DatabasePath = databasePath ?? DefaultDatabasePath();
        var directory = Path.GetDirectoryName(DatabasePath);
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);

        _connectionString = new SqliteConnectionStringBuilder
        {
            DataSource = DatabasePath,
            Mode = SqliteOpenMode.ReadWriteCreate,

            // Pooling off, deliberately. With it on (the default) the provider
            // keeps the file handle open after Dispose, so the .db stays locked
            // for the life of the process — which makes the file impossible to
            // move, back up or delete while the app runs, and is baffling when
            // you hit it. Saves and loads happen a few times a minute at most,
            // so the cost of reopening a local file is irrelevant here.
            Pooling = false,
        }.ToString();

        Initialise();
    }

    public string DatabasePath { get; }

    /// <summary>
    /// %APPDATA%\Tupista on Windows, ~/.config/Tupista on macOS and Linux.
    /// SpecialFolder.ApplicationData is the portable spelling of "this user's
    /// settings live here", which matters because this project targets both.
    /// </summary>
    public static string DefaultDatabasePath() => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData,
                                  Environment.SpecialFolderOption.Create),
        "Tupista", "boards.db");

    private void Initialise()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();

        // IF NOT EXISTS makes this safe to run on every start, which is the
        // simplest migration strategy there is. When the schema needs to change
        // later, this is where a version check would go.
        command.CommandText = """
            CREATE TABLE IF NOT EXISTS boards (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                name        TEXT    NOT NULL,
                givens      TEXT    NOT NULL,
                current     TEXT    NOT NULL,
                marks       TEXT    NOT NULL,
                created_utc TEXT    NOT NULL,
                updated_utc TEXT    NOT NULL
            );

            CREATE TABLE IF NOT EXISTS settings (
                key   TEXT PRIMARY KEY,
                value TEXT NOT NULL
            );
            """;
        command.ExecuteNonQuery();
    }

    /// <summary>A stored preference, or <paramref name="fallback"/> if unset.</summary>
    public string GetSetting(string key, string fallback)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = "SELECT value FROM settings WHERE key = $key;";
        command.Parameters.AddWithValue("$key", key);
        return command.ExecuteScalar() as string ?? fallback;
    }

    /// <summary>
    /// Stores a preference. UPSERT ("insert, or update if the key is taken")
    /// avoids a read-then-write race and is a single statement.
    /// </summary>
    public void SetSetting(string key, string value)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO settings (key, value) VALUES ($key, $value)
            ON CONFLICT(key) DO UPDATE SET value = excluded.value;
            """;
        command.Parameters.AddWithValue("$key", key);
        command.Parameters.AddWithValue("$value", value);
        command.ExecuteNonQuery();
    }

    private SqliteConnection Open()
    {
        var connection = new SqliteConnection(_connectionString);
        connection.Open();
        return connection;
    }

    /// <summary>Every saved board, most recently touched first.</summary>
    public IReadOnlyList<SavedBoard> LoadAll()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            SELECT id, name, givens, current, marks, created_utc, updated_utc
            FROM boards ORDER BY updated_utc DESC;
            """;

        var boards = new List<SavedBoard>();
        using var reader = command.ExecuteReader();
        while (reader.Read()) boards.Add(Read(reader));
        return boards;
    }

    /// <summary>
    /// Inserts a new board, or updates the existing one when Id is non-zero.
    /// Returns the board with its Id and timestamps filled in.
    /// </summary>
    public SavedBoard Save(SavedBoard board)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();

        var now = DateTime.UtcNow;
        if (board.Id == 0)
        {
            command.CommandText = """
                INSERT INTO boards (name, givens, current, marks, created_utc, updated_utc)
                VALUES ($name, $givens, $current, $marks, $created, $updated);
                SELECT last_insert_rowid();
                """;
            command.Parameters.AddWithValue("$created", Stamp(now));
        }
        else
        {
            command.CommandText = """
                UPDATE boards
                SET name = $name, givens = $givens, current = $current,
                    marks = $marks, updated_utc = $updated
                WHERE id = $id;
                SELECT $id;
                """;
            command.Parameters.AddWithValue("$id", board.Id);
        }

        command.Parameters.AddWithValue("$name", board.Name);
        command.Parameters.AddWithValue("$givens", board.Givens);
        command.Parameters.AddWithValue("$current", board.Current);
        command.Parameters.AddWithValue("$marks", EncodeMarks(board.Marks));
        command.Parameters.AddWithValue("$updated", Stamp(now));

        var id = Convert.ToInt64(command.ExecuteScalar());
        return board with
        {
            Id = id,
            CreatedUtc = board.Id == 0 ? now : board.CreatedUtc,
            UpdatedUtc = now,
        };
    }

    public void Delete(long id)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = "DELETE FROM boards WHERE id = $id;";
        command.Parameters.AddWithValue("$id", id);
        command.ExecuteNonQuery();
    }

    // --- serialisation ------------------------------------------------------

    // Round-trip format for timestamps ("o"): sorts correctly as text, which is
    // what lets ORDER BY updated_utc work on a TEXT column. SQLite has no real
    // date type, so the format has to do that job.
    private static string Stamp(DateTime utc) => utc.ToString("o");

    /// <summary>
    /// 81 mark bitmasks as comma-separated numbers. Deliberately plain text:
    /// a blob would be smaller, but this stays readable in any SQLite browser,
    /// which is worth far more while the format is still settling.
    /// </summary>
    private static string EncodeMarks(ushort[] marks) => string.Join(',', marks);

    private static ushort[] DecodeMarks(string text)
    {
        var marks = new ushort[SavedBoard.CellCount];
        if (string.IsNullOrWhiteSpace(text)) return marks;

        var parts = text.Split(',');
        for (var i = 0; i < marks.Length && i < parts.Length; i++)
            ushort.TryParse(parts[i], out marks[i]);
        return marks;
    }

    private static SavedBoard Read(SqliteDataReader reader) => new(
        reader.GetInt64(0),
        reader.GetString(1),
        reader.GetString(2),
        reader.GetString(3),
        DecodeMarks(reader.GetString(4)),
        DateTime.Parse(reader.GetString(5), null, System.Globalization.DateTimeStyles.RoundtripKind),
        DateTime.Parse(reader.GetString(6), null, System.Globalization.DateTimeStyles.RoundtripKind));
}
