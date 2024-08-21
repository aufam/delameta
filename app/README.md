## Delameta Demo App

### [Readme](https://github.com/aufam/delameta/blob/main/app/readme.cpp)
#### Endpoint: `/`
* Method: `GET`
* Description: Returns an HTML page serving as the homepage of the API.
* Response content type: `text/html`

#### Endpoint: `/readme`
* Method: `GET`
* Description: Returns this [README](https://github.com/aufam/delameta/blob/main/app/README.md)
* Response content type: `text/markdown`

### [Example](https://github.com/aufam/delameta/blob/main/app/example.cpp)

#### Endpoint: `/hello`
* Method: `GET`
* Description: Hello world example.
* Response content type: `text/html`
* Response body: `Hello world from delameta/${DELAMETA_VERSION}`

#### Endpoint: `/body`
* Method: `POST`
* Description: Example endpoint that exposes the request body to the handler
* Response content type: `text/plain`
* Response status: `OK` if request body is not empty, otherwise `Bad Request` 
* Response body: echo from request body

#### Endpoint: `/foo`
* Method: `POST`
* Description: Example demonstrating:
  - Dependency injection for authorization
  - Query argument with default value
  - JSON serialization/deserialization
* Queries:
  - `add` (int): default value is 20
* Headers:
  - `Authorization: Bearer 1234`
* Body:
  ```json
  {
    "num": "number",
    "text": "string"
  }
  ```
* Response status:
  - `Unauthorized` if authentication fails
  - `Bad Request` if the content type is not JSON
  - `Internal Server Error` if the body cannot be deserialized into `Foo`
  - `OK` otherwise
* Example request:
  - Request path: `/foo?add=30`
  - Request Body:
    ```json
    {
      "num": 42,
      "text": "foo"
    }
    ```
  - Response Body:
    ```json
    {
      "num": 72,
      "text": "foo: Bearer 1234"
    }
    ```
  - Description:
    * num: The value from the request body num added to the add query parameter (42 + 30 = 72).
    * text: The value from the request body text concatenated with the Authorization header token (in this example, foo + : Bearer 1234).

#### Endpoint: `/methods`
* Method: `POST` | `GET`
* Description: example for exposing the request method to the handler
* Response status:
  - `Method Not Allowed` if method is not `POST` nor `GET`
  - `OK` otherwise
* Response body: `Example GET method` if method is `GET` otherwise `Example POST method`

#### Endpoint `/routes`
* Method: `GET`
* Description: get all routes/endpoints and its method as JSON list

#### Endpoint `/headers`
* Method: `GET`
* Description: get all request headers as JSON

#### Endpoint `/queries`
* Method: `GET`
* Description: get all request queries as JSON

#### Endpoint `/redirect`
* Method: `GET`|`POST`|`PUT`|`PATCH`|`HEAD`|`TRACE`|`DELETE`|`OPTIONS`
* Description: redirect the request to the url given by the query `url`
* Queries:
  - `url` url to redirect to

#### Endpoint `/delete_route`
* Method: `DELETE`
* Description: delete a router that is specified by query `path`
* Queries:
  - `path` the router's path

### [File Handler](https://github.com/aufam/delameta/blob/main/app/file_handler.cpp)

#### Endpoint `/download`
* Method: `GET`
* Description: download a file specified by the query `filename`
* Queries:
  - `filename` filename relative to where the app is run
* Response status:
  - `Bad Request` if the file doesn't exist
  - `Internal Server Error` if reading the file fails
  - `OK` otherwise
* Response content type: aligns with the file
* Response body: the content of the file

#### Endpoint `/upload`
* Method: `PUT`
* Description: Upload a file to the location specified by the query `filename` 
* Queries:
  - `filename` filename relative to where the app is run
* Response status:
  - `Internal Server Error` if writing the file fails
  - `OK` otherwise

#### Endpoint `/route_file`
* Method: `POST`
* Description: Create a new endpoint
* Queries:
  - `route` the endpoint route to access the file
  - `filename` filename relative to where the app is run
* Response status:
  - `Conflict` if the endpoint is already exists
  - `OK` otherwise

### [Serial Handler](https://github.com/aufam/delameta/blob/main/app/serial_handler.cpp)

#### Endpoint `/serial`
* Method: `POST`
* Description: Open a serial port, send the request body, and receive the response data
* Queries:
  - `port` (string): serial port. default value is `auto` for automatically detecting the available serial port
  - `baud` (int): baud rate. default value is `9600`
  - `timeout` (int): receive timeout in seconds. default value is `5`
* Request body: octet-stream of data
  - `Internal Server Error` if opening the serial port fails
  - `OK` otherwise
* Response content type: `application/octet-stream`
* Response body: the received data of the serial port
