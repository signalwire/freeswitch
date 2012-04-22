// To get unixodbc to work on Mono Linux, you may need to create an odbc32.dll link: ln -s /lib64/libodbc.so /lib64/odbc32.dll
open System
open System.Data
open FreeSWITCH

type QueryResult = { dialstring: string; group: string; acctcode: string; limit: int; translated: string }

module easyroute =
    // Basic config
    let defaultStr def = function null | "" -> def | s -> s
    let getAppSetting (name:string) = match Configuration.ConfigurationManager.AppSettings.Get name with null -> "" | x -> x
    let connString      = getAppSetting "connectionString"
    let defaultProfile  = getAppSetting "defaultProfile"
    let defaultGateway  = getAppSetting "defaultGateway"
    let query           = getAppSetting "query"
    let configOk = [ connString; defaultProfile; defaultGateway; query; ] |> List.forall (String.IsNullOrEmpty >> not)
    let numberRegexFilter = defaultStr "[^0-9#]" (getAppSetting "numberRegexFilter")
    
    // Determine if ODBC driver quotes parameters properly - MySQL < 3.51.16 apparently does not
    // We'll select the string "'" -- if quoting works, we'll get ' back. Otherwise, it'll fail, and we'll refuse to load
    // Error 1064 seems to be the syntax error code MySQL returns. Otherwise, the exception will still stop it from loading, just less gracefully.
    let odbcOk = use conn = new Odbc.OdbcConnection(connString)
                 use comm = new Odbc.OdbcCommand("SELECT ?", conn)
                 comm.Parameters.AddWithValue("@test", "'") |> ignore
                 conn.Open()
                 try string (comm.ExecuteScalar()) = "'" 
                 with :? Odbc.OdbcException as ex when ex.Errors.Count > 0 && ex.Errors.[0].NativeError = 1064 -> false
                
    let formatDialstring number gateway profile separator = 
        match separator with 
        | None   -> sprintf "%s/%s%s"   profile number gateway
        | Some s -> sprintf "%s/%s%s%s" profile number s gateway

    let getDefaultResult number sep = { 
        dialstring = formatDialstring number defaultGateway defaultProfile sep;
        limit = 9999; group = ""; acctcode = ""; translated = number; }
        
    let readResult (r: IDataReader) number sep =
        let gw          = defaultStr defaultGateway <| r.GetString(0)
        let group       = r.GetString(1)
        let limit       = match r.GetInt32(2) with 0 -> 9999 | x -> x
        let profile     = defaultStr defaultProfile <| r.GetString(3)
        let acctcode    = r.GetString(4)
        let translated  = r.GetString(5)
        let dialstring = formatDialstring number gw profile sep
        { dialstring = dialstring; limit = limit; group = group; acctcode = acctcode; translated = translated; }

    let regexOpts = Text.RegularExpressions.RegexOptions.Compiled ||| Text.RegularExpressions.RegexOptions.CultureInvariant
    let lookup (number: string) sep =
        try
            let number = if numberRegexFilter = "(?!.)" then number else Text.RegularExpressions.Regex.Replace(number, numberRegexFilter, "", regexOpts)
            use conn = new Odbc.OdbcConnection(connString)
            use comm = new Odbc.OdbcCommand(query, conn)
            comm.Parameters.AddWithValue("@number", number) |> ignore
            conn.Open()
            use reader = comm.ExecuteReader CommandBehavior.SingleRow
            match reader.Read() with
            | true  -> readResult reader number sep
            | false -> Log.WriteLine(LogLevel.Error, "No records for {0}; setting default route.", number)
                       getDefaultResult number sep
        with ex -> Log.WriteLine(LogLevel.Error, "Exception getting route for {0}. Setting default route. Exception: {1}", number, ex.ToString())
                   getDefaultResult number sep

    // Returns tuple: number * separator option * field option
    let parseArgs args = 
        let args = String.split [' '] args
        let num = List.hd args
        let opt = Map.of_list (List.tl args |> List.map (fun x -> match x.Split([|'='|], 2) with 
                                                                  | [|n;v|] -> n, Some v 
                                                                  | arr     -> arr.[0], None))
        (num, defaultArg (opt.TryFind "separator") (Some "@"), defaultArg (opt.TryFind "field") (None))
open easyroute

type EasyRoute() =
    interface ILoadNotificationPlugin with
        member x.Load() = 
            if not configOk then Log.WriteLine(LogLevel.Alert, "EasyRoute configuration is missing values.")
            if not odbcOk   then Log.WriteLine(LogLevel.Critical, "ODBC driver doesn't handle quoting properly; upgrade driver.")
            configOk && odbcOk
        
    interface IApiPlugin with
        member x.ExecuteBackground ctx = 
            Log.WriteLine(LogLevel.Error, "Background execution not supported for EasyRoute.")
        member x.Execute ctx = 
            let num, sep, field = parseArgs ctx.Arguments
            let res = lookup num sep
            let sw = ctx.Stream.Write 
            match field with
            | None              -> sw "Number    \tLimit     \tGroup    \tAcctCode  \tDialstring\n"
                                   sw (sprintf "%-10s\t%-10d\t%-10s\t%-10s\t%s\n" res.translated res.limit res.group res.acctcode res.dialstring)
            | Some "dialstring" -> sw res.dialstring
            | Some "translated" -> sw res.translated
            | Some "limit"      -> sw (string res.limit)
            | Some "group"      -> sw res.group
            | Some "acctcode"   -> sw res.acctcode 
            | _                 -> sw "Invalid input!\n"
        
    interface IAppPlugin with
        member x.Run ctx = 
            let num, sep, field = parseArgs ctx.Arguments
            let res = lookup num sep
            [ "easy_destnum", res.translated; "easy_dialstring", res.dialstring; "easy_group", res.group; "easy_limit", string res.limit; "easy_acctcode", res.acctcode]
                |> List.iter ctx.Session.SetVariable
