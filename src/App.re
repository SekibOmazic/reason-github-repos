/* http://blog.jenkster.com/2016/06/how-elm-slays-a-ui-antipattern.html */
module RemoteData = {
  type t('e, 'a) =
    | NotAsked
    | Loading
    | Failure('e)
    | Success('a);
};

module Http = {
  type error =
    | Timeout
    | NetworkError
    | BadStatus(int, string);
  let getJSON = (url: string) : Js.Promise.t(Js.Result.t(Js.Json.t, error)) =>
    Js.Promise.(
      Fetch.fetch(url)
      |> then_(value => {
           Js.log(value);
           let status = value |> Fetch.Response.status;
           let statusText = value |> Fetch.Response.statusText;
           switch status {
           | 0 => Js.Result.Error(NetworkError) |> resolve /* can this happen here? */
           | 408 => Js.Result.Error(Timeout) |> resolve
           | status when status < 200 =>
             Js.Result.Error(BadStatus(status, statusText)) |> resolve
           | status when status >= 300 =>
             BadStatus(status, statusText)
             |> (err => Js.Result.Error(err))
             |> resolve
           | _ =>
             value
             |> Fetch.Response.json
             |> then_(json => Js.Result.Ok(json) |> resolve)
           };
         })
      |> catch(_err => Js.Result.Error(NetworkError) |> resolve)
    );
};

type repo = {
  id: int,
  owner: string,
  name: string,
  full_name: string,
  stars: int,
  html_url: string,
  description: option(string),
  fork: bool
};

type state = {
  username: string,
  repos: RemoteData.t(string, list(repo))
};

type action =
  | ChangeUsername(string)
  | LoadRepos
  | ReposLoaded(list(repo))
  | ReposFailedToLoad(string);

let forkPath = "M8 1a1.993 1.993 0 0 0-1 3.72V6L5 8 3 6V4.72A1.993 1.993 0 0 0 2 1a1.993 1.993 0 0 0-1 3.72V6.5l3 3v1.78A1.993 1.993 0 0 0 5 15a1.993 1.993 0 0 0 1-3.72V9.5l3-3V4.72A1.993 1.993 0 0 0 8 1zM2 4.2C1.34 4.2.8 3.65.8 3c0-.65.55-1.2 1.2-1.2.65 0 1.2.55 1.2 1.2 0 .65-.55 1.2-1.2 1.2zm3 10c-.66 0-1.2-.55-1.2-1.2 0-.65.55-1.2 1.2-1.2.65 0 1.2.55 1.2 1.2 0 .65-.55 1.2-1.2 1.2zm3-10c-.66 0-1.2-.55-1.2-1.2 0-.65.55-1.2 1.2-1.2.65 0 1.2.55 1.2 1.2 0 .65-.55 1.2-1.2 1.2z";

let starPath = "M15.2 40.6c-.2 0-.4-.1-.6-.2-.4-.3-.5-.7-.4-1.1l3.9-12-10.2-7.5c-.4-.3-.5-.7-.4-1.1s.5-.7 1-.7h12.7L25 5.9c.1-.4.5-.7 1-.7s.8.3 1 .7L30.9 18h12.7c.4 0 .8.2 1 .6s0 .9-.4 1.1L34 27.1l3.9 12c.1.4 0 .9-.4 1.1s-.8.3-1.2 0L26 33l-10.2 7.4c-.2.1-.4.2-.6.2zM26 30.7c.2 0 .4.1.6.2l8.3 6.1-3.2-9.8c-.1-.4 0-.9.4-1.1l8.3-6.1H30.1c-.4 0-.8-.3-1-.7L26 9.5l-3.2 9.8c-.1.4-.5.7-1 .7H11.5l8.3 6.1c.4.3.5.7.4 1.1L17.1 37l8.3-6.1c.2-.1.4-.2.6-.2z";

let str = ReasonReact.stringToElement;

let valueFromEvent = evt : string => (
                                       evt
                                       |> ReactEventRe.Form.target
                                       |> ReactDOMRe.domElementToObj
                                     )##value;

let parseResponse = (json: Js.Json.t) : list(repo) =>
  json
  |> Json.Decode.array(json =>
       Json.Decode.{
         id: json |> field("id", int),
         owner:
           json |> field("owner", owner => owner |> field("login", string)),
         name: json |> field("name", string),
         full_name: json |> field("full_name", string),
         stars: json |> field("stargazers_count", int),
         html_url: json |> field("html_url", string),
         description: json |> optional(field("description", string)),
         fork: json |> field("fork", bool)
       }
     )
  |> Array.to_list;

let messageFromError = (err: Http.error) : string =>
  switch err {
  | Http.Timeout => "connection timeout"
  | Http.NetworkError => "No internet connection!"
  | Http.BadStatus(status, statusText) =>
    "request failed with status "
    ++ string_of_int(status)
    ++ ". Message: "
    ++ statusText
  };

let fetchRepos = (username, send) =>
  Js.Promise.(
    Http.getJSON(
      "https://api.github.com/users/"
      ++ username
      ++ "/repos?type=all&sort=updated"
    )
    |> then_(result =>
         (
           switch result {
           | Js.Result.Ok(json) =>
             json |> parseResponse |> (repos => ReposLoaded(repos)) |> send
           | Js.Result.Error(err) =>
             err |> messageFromError |> (msg => ReposFailedToLoad(msg)) |> send
           }
         )
         |> resolve
       )
    |> catch(_err =>
         ReposFailedToLoad("could not parse json") |> send |> resolve
       )
  )
  |> ignore;

let renderDesc = (desc: option(string)) =>
  switch desc {
  | Some(str) => str
  | None => "No desciption :("
  };

let items = (repos: list(repo)) =>
  repos
  |> List.map(repo =>
       <li
         key=(string_of_int(repo.id))
         className="flex flex-row justify-between w-full border-b">
         <div className="px-6 py-4">
           <div
             className="flex flex-row items-center font-mono text-base mb-2">
             <a
               href=repo.html_url
               target="_blank"
               className="no-underline text-orange-dark mr-2">
               (repo.full_name |> str)
             </a>
             (
               repo.fork ?
                 <svg
                   className="fill-current text-green-dark h-4 w-4"
                   viewBox="0 0 10 16">
                   <path fillRule="evenodd" d=forkPath />
                 </svg> :
                 ReasonReact.nullElement
             )
           </div>
           <p className="font-mono text-orange-lighter text-xs">
             (repo.description |> renderDesc |> str)
           </p>
         </div>
         <div className="flex flex-row items-center px-6 py-4">
           <span
             className="font-mono inline-block rounded-full px-1 py-1 text-sm text-orange-dark mr-1">
             (repo.stars |> string_of_int |> str)
           </span>
           <svg
             className="fill-current text-orange-light h-4 w-4 inline-block"
             viewBox="0 0 50 50">
             <path d=starPath />
           </svg>
         </div>
       </li>
     );

let component = ReasonReact.reducerComponent("App");

let make = _children => {
  ...component,
  initialState: () => {username: "", repos: RemoteData.NotAsked},
  reducer: (action, state) =>
    switch action {
    | ChangeUsername(username) =>
      ReasonReact.Update({username, repos: NotAsked})
    | LoadRepos =>
      ReasonReact.UpdateWithSideEffects(
        {...state, repos: RemoteData.Loading},
        (self => fetchRepos(state.username, self.send))
      )
    | ReposLoaded(repos) =>
      ReasonReact.Update({...state, repos: RemoteData.Success(repos)})
    | ReposFailedToLoad(msg) =>
      ReasonReact.Update({...state, repos: RemoteData.Failure(msg)})
    },
  render: ({state, send}) =>
    <div
      className="h-screen w-full bg-yellow-lighter flex flex-col justify-start items-center overflow-scroll">
      <input
        className="mt-8 shadow border font-mono bg-yellow-lightest appearance-none py-2 px-3 text-orange-darker rounded w-3/5"
        placeholder="Enter a Gihtub username ..."
        value=state.username
        onKeyDown=(
          event =>
            if (ReactEventRe.Keyboard.keyCode(event) == 13) {
              ReactEventRe.Keyboard.preventDefault(event);
              send(LoadRepos);
            }
        )
        onChange=(
          event =>
            event |> valueFromEvent |> (value => ChangeUsername(value)) |> send
        )
      />
      (
        switch state.repos {
        | NotAsked => ReasonReact.nullElement
        | Loading =>
          <p className="mt-8 font-mono text-orange-dark text-lg">
            (str("Loading..."))
          </p>
        | Success(repos) =>
          if (List.length(repos) > 0) {
            <div
              className="bg-white shadow rounded flex overflow-scroll w-3/5 mb-8 mt-8">
              <ul className="appearance-none p-0 w-full text-grey-darker">
                (ReasonReact.arrayToElement(repos |> items |> Array.of_list))
              </ul>
            </div>;
          } else {
            <p className="mt-8 font-mono text-orange-dark text-lg">
              (str(state.username ++ " doesn't have any public repositories"))
            </p>;
          }
        | Failure(err) =>
          <p className="mt-8 font-mono text-orange-dark text-lg">
            (str(err))
          </p>
        }
      )
    </div>
};
